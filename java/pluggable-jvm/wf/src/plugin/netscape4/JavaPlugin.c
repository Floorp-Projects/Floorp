/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: JavaPlugin.c,v 1.2 2001/07/12 19:58:21 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

/* -*- Mode: C; tab-width: 4; -*- */
/******************************************************************************
 * Copyright (c) 1996 Netscape Communications. All rights reserved.
 ******************************************************************************/
/*
 * UnixShell.c
 *
 * Netscape Client Plugin API
 * - Function that need to be implemented by plugin developers
 *
 * This file defines a "Template" plugin that plugin developers can use
 * as the basis for a real plugin.  This shell just provides empty
 * implementations of all functions that the plugin can implement
 * that will be called by Netscape (the NPP_xxx methods defined in 
 * npapi.h). 
 *
 * dp Suresh <dp@netscape.com>
 *
 */

#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "jvmp.h"
#include "common.h"
#include "shmtran.h"

#define MAX_INSTANCES 200
#define ADD_INST(inst) for(_idx=0; _idx<MAX_INSTANCES; _idx++) \
                     if (!g_instances[_idx]) { g_instances[_idx] = inst; break; } \
                     if (_idx == MAX_INSTANCES) fprintf(stderr, "Leaking PluginInsances?\n");
#define DEL_INST(inst) for(_idx=0; _idx<MAX_INSTANCES; _idx++) \
                     if (g_instances[_idx] == inst) { g_instances[_idx] = NULL; break; }

static JVMP_RuntimeContext* g_jvmp_context = NULL;
static void*                g_dll = NULL;
static JavaVM*              g_jvm = NULL;
static JVMP_CallingContext* g_ctx;
static GlobalData           g_data;
static PluginInstance*      g_instances[MAX_INSTANCES];
static int                  _idx;
static int                  g_stopped = 0;
static int                  g_inited = 0;

#ifdef XP_UNIX 
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/VendorS.h> /* really bad */

static Atom                 g_request_handler_atom;
#endif

/*
 *     Protypes of utility functions
 */
static NPError DoDirtyInit();
static jint loadPluginDLL();
static jint initJVM(JavaVM** jvm, JVMP_CallingContext** ctx, 
					JavaVMInitArgs* vm_args);
static jint PutParams(jint id, char** buf, int* argc, int* len);
static PluginInstance*  GetInstance(jint id);
static void SetDocBase(PluginInstance* inst, char* buf, int len);
static int GetProxyForURL(void* handle, char* url, char** proxy)
{
  PluginInstance* inst = (PluginInstance*)handle;
  return JNI_TRUE;
}

static int ShowDocument(jint id, char* url, char* target)
{
   PluginInstance* inst;
   inst = GetInstance(id);
   if (!inst) return JNI_FALSE;
   NPN_GetURL(inst->m_peer, url, target);
   return JNI_TRUE;
}

static int ShowStatus(jint id, char* status)
{
  PluginInstance* inst;
  inst = GetInstance(id);
  if (!inst) return JNI_FALSE;
  NPN_Status(inst->m_peer, status);
  return JNI_TRUE;
}


/***********************************************************************
 *
 * Implementations of plugin API functions
 *
 ***********************************************************************/

char*
NPP_GetMIMEDescription(void)
{
  printf("NPP_GetMIMEDescription\n");
  return 
	  "application/x-java-vm::Java(tm) Plug-in"
	  ";application/x-java-applet::Java(tm) Plug-in";
}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
	NPError err = NPERR_NO_ERROR;
	
	switch (variable) {
		case NPPVpluginNameString:
			*((char **)value) = "Java plugin";
			break;
		case NPPVpluginDescriptionString:
			*((char **)value) =
				"Flexible Java plugin based on Waterfall API";
			break;
		default:
			err = NPERR_GENERIC_ERROR;
	}
	return err;
}

NPError
NPP_Initialize(void)
{
  JavaVMInitArgs vm_args;
  jint ext_id;
 
  if (g_inited) 
	  return NPERR_NO_ERROR;
  if (g_jvmp_context != NULL) 
	return NPERR_NO_ERROR;
  JVMP_ShmInit(1);
  if (loadPluginDLL() != JNI_TRUE)
	return NPERR_GENERIC_ERROR;
  memset(&vm_args, 0, sizeof(vm_args));
  vm_args.version = JNI_VERSION_1_2;
  (g_jvmp_context->JVMP_GetDefaultJavaVMInitArgs)(&vm_args);
  if (initJVM(&g_jvm, &g_ctx, &vm_args) != JNI_TRUE) 
    return NPERR_GENERIC_ERROR;
  if ((g_jvmp_context->JVMP_RegisterExtension)
	  (g_ctx, 
	   "/ws/wf/build/unix/libwf_netscape4.so", 
	   &ext_id) != JNI_TRUE)
	return NPERR_GENERIC_ERROR;
  return DoDirtyInit();
}


void
NPP_Shutdown(void)
{
  g_stopped = 1;
}


NPError 
NPP_New(NPMIMEType pluginType,
		NPP instance,
		uint16 mode,
		int16 argc,
		char* argn[],
		char* argv[],
		NPSavedData* saved)
{
  PluginInstance* This;
  int i;

  if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;
  
  instance->pdata = malloc(sizeof(PluginInstance));
	
  This = (PluginInstance*) instance->pdata;
  
  if (This == NULL)
	return NPERR_OUT_OF_MEMORY_ERROR;
  This->m_argn = (char**)malloc(sizeof(char*)*(argc+1));
  This->m_argv = (char**)malloc(sizeof(char*)*(argc+1));
  This->m_peer = instance;
  for(i=0; i<argc; i++)
	{
	  (This->m_argn)[i] = strdup(argn[i]);
	  (This->m_argv)[i] = strdup(argv[i]);
	}; 
  //NPN_GetURL(This->m_peer, "javascript:document.location", 
  //			 NULL);
  This->m_argn[i] = strdup("docbase");
  This->m_argv[i] = strdup("http://java.sun.com");  
  argc++;
  This->m_argc = argc;
  This->m_wid = 0;
  ADD_INST(This);
  if ((g_jvmp_context->JVMP_CreatePeer)(g_ctx, 
										PV_MOZILLA4, 
										0,
										&(This->m_id)) != JNI_TRUE)
	return NPERR_GENERIC_ERROR;
  if ((g_jvmp_context->JVMP_SendEvent)(g_ctx, 
									   This->m_id, 
									   (jint)PE_SETTYPE,
									   (jlong)PT_APPLET) != JNI_TRUE)
	return NPERR_GENERIC_ERROR;
  /* async as it calls browser back */
  if ((g_jvmp_context->JVMP_PostEvent)(g_ctx, 
									   This->m_id, 
									   (jint)PE_NEWPARAMS,
									   (jlong)0) != JNI_TRUE)
	return NPERR_GENERIC_ERROR;
  return NPERR_NO_ERROR;
}


NPError 
NPP_Destroy(NPP instance, NPSavedData** save)
{
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	This = (PluginInstance*) instance->pdata;

	/* PLUGIN DEVELOPERS:
	 *	If desired, call NP_MemAlloc to create a
	 *	NPSavedDate structure containing any state information
	 *	that you want restored if this plugin instance is later
	 *	recreated.
	 */

	if (This != NULL) {
		g_jvmp_context->JVMP_SendEvent(g_ctx, 
									   This->m_id, 
									   (jint)PE_STOP,
									   0);
		g_jvmp_context->JVMP_UnregisterWindow(g_ctx, 
											  This->m_wid);
		/*g_jvmp_context->JVMP_SendEvent(g_ctx, 
									   This->m_id, 
									   (jint)PE_DESTROY,
									   0); */
		g_jvmp_context->JVMP_DestroyPeer(g_ctx,
										 This->m_id);
		// XXX: leak, create DestroyInstance() function
		DEL_INST(This);
		free(instance->pdata);
		instance->pdata = NULL;
	}

	return NPERR_NO_ERROR;
}



NPError 
NPP_SetWindow(NPP instance, NPWindow* rwin)
{
  PluginInstance* This;
  JVMP_DrawingSurfaceInfo w;
  jint ID;
  
  if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;
  
  if (rwin == NULL)
	return NPERR_NO_ERROR;
	
  This = (PluginInstance*) instance->pdata;

  w.window =  (struct JPluginWindow*)rwin->window;
  w.x = rwin->x;
  w.y = rwin->y;
  w.width = rwin->width;
  w.height = rwin->height;
#ifdef XP_UNIX
  XSync(g_data.m_dpy, 0); /* otherwise JDK display connection won't see it */
#endif
  if ((g_jvmp_context->JVMP_RegisterWindow)(g_ctx, &w, &ID) != JNI_TRUE) 
	return NPERR_GENERIC_ERROR;
  if ((g_jvmp_context->JVMP_SendEvent)(g_ctx, 
									   This->m_id, 
									   (jint)PE_SETWINDOW,
									   (jlong)ID) != JNI_TRUE)
	return NPERR_GENERIC_ERROR;
  return NPERR_NO_ERROR;
}


NPError 
NPP_NewStream(NPP instance,
			  NPMIMEType type,
			  NPStream *stream, 
			  NPBool seekable,
			  uint16 *stype)
{
	NPByteRange range;
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	This = (PluginInstance*) instance->pdata;

	return NPERR_NO_ERROR;
}


/* PLUGIN DEVELOPERS:
 *	These next 2 functions are directly relevant in a plug-in which
 *	handles the data in a streaming manner. If you want zero bytes
 *	because no buffer space is YET available, return 0. As long as
 *	the stream has not been written to the plugin, Navigator will
 *	continue trying to send bytes.  If the plugin doesn't want them,
 *	just return some large number from NPP_WriteReady(), and
 *	ignore them in NPP_Write().  For a NP_ASFILE stream, they are
 *	still called but can safely be ignored using this strategy.
 */

int32 STREAMBUFSIZE = 0X0FFFFFFF; /* If we are reading from a file in NPAsFile
				   * mode so we can take any size stream in our
				   * write call (since we ignore it) */

int32 
NPP_WriteReady(NPP instance, NPStream *stream)
{
	PluginInstance* This;
	if (instance != NULL)
		This = (PluginInstance*) instance->pdata;

	return STREAMBUFSIZE;
}


int32 
NPP_Write(NPP instance, NPStream *stream, 
		  int32 offset, int32 len, void *buffer)
{
  
  fprintf(stderr, "NPP_WRITE %d\n", (int)len);
  if (instance != NULL)
	{
		PluginInstance* This = (PluginInstance*) instance->pdata;
		SetDocBase(This, (char*)buffer, len);
		fprintf(stderr, "got base notification: %s", This->m_docbase);
	}	
	return len;		/* The number of bytes accepted */
}


NPError 
NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason)
{
	PluginInstance* This;

	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;
	This = (PluginInstance*) instance->pdata;

	return NPERR_NO_ERROR;
}


void 
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
	PluginInstance* This;
	if (instance != NULL)
		This = (PluginInstance*) instance->pdata;
}

void NPP_URLNotify(NPP         instance, 
                   const       char* url,
                   NPReason    reason, 
                   void*       notifyData)
{
  fprintf(stderr, "URLNotify\n");
}


void 
NPP_Print(NPP instance, NPPrint* printInfo)
{
	if(printInfo == NULL)
		return;

	if (instance != NULL) {
		PluginInstance* This = (PluginInstance*) instance->pdata;
	
		if (printInfo->mode == NP_FULL) {
		    /*
		     * PLUGIN DEVELOPERS:
		     *	If your plugin would like to take over
		     *	printing completely when it is in full-screen mode,
		     *	set printInfo->pluginPrinted to TRUE and print your
		     *	plugin as you see fit.  If your plugin wants Netscape
		     *	to handle printing in this case, set
		     *	printInfo->pluginPrinted to FALSE (the default) and
		     *	do nothing.  If you do want to handle printing
		     *	yourself, printOne is true if the print button
		     *	(as opposed to the print menu) was clicked.
		     *	On the Macintosh, platformPrint is a THPrint; on
		     *	Windows, platformPrint is a structure
		     *	(defined in npapi.h) containing the printer name, port,
		     *	etc.
		     */

			void* platformPrint =
				printInfo->print.fullPrint.platformPrint;
			NPBool printOne =
				printInfo->print.fullPrint.printOne;
			
			/* Do the default*/
			printInfo->print.fullPrint.pluginPrinted = FALSE;
		}
		else {	/* If not fullscreen, we must be embedded */
		    /*
		     * PLUGIN DEVELOPERS:
		     *	If your plugin is embedded, or is full-screen
		     *	but you returned false in pluginPrinted above, NPP_Print
		     *	will be called with mode == NP_EMBED.  The NPWindow
		     *	in the printInfo gives the location and dimensions of
		     *	the embedded plugin on the printed page.  On the
		     *	Macintosh, platformPrint is the printer port; on
		     *	Windows, platformPrint is the handle to the printing
		     *	device context.
		     */

			NPWindow* printWindow =
				&(printInfo->print.embedPrint.window);
			void* platformPrint =
				printInfo->print.embedPrint.platformPrint;
		}
	}
}


/* here our private functions */
char* GetDlLibraryName(const char* path, 
					   const char* name)
{
  char* result = 
	(char*)malloc(strlen(path) + strlen(name) + 10);
  /* XXX: Unix implementation */
  sprintf(result, "%s/lib%s.so", path, name);
  return result;
}

void* LoadDlLibrary(char* name)
{
  /* XXX: Unix implementation */
  return dlopen(name, RTLD_LAZY);
}

void GetDlErrorText(char* buf)
{
  char* err;
  int len;
  
  err = dlerror();
  if (err) 
	memcpy(buf, err, strlen(err));  
}

void* FindDlSymbol(void* handle, const char* name)
{
  return dlsym(handle, name);
}

static jint loadPluginDLL()
{
  const char*            plugin_home;
  char errorMsg[1024] = "<unknown; can't get error>";
  char* plugin_dll;
  JVMP_GetPlugin_t fJVMP_GetPlugin;

  plugin_home = getenv("JAVA_PLUGIN_HOME");
  if (plugin_home == NULL) {
    fprintf(stderr, "Env variable JAVA_PLUGIN_HOME not set");
    return JNI_FALSE;
  }
  plugin_dll = GetDlLibraryName(plugin_home, "jvmp_shm");
  
  g_dll = LoadDlLibrary(plugin_dll);
  free(plugin_dll);
  if (!g_dll) 
    {
	  GetDlErrorText(errorMsg);
      fprintf(stderr, "Cannot load plugin DLL: %s", errorMsg);
	  return JNI_FALSE;
    }
  fJVMP_GetPlugin = 
    (JVMP_GetPlugin_t) FindDlSymbol(g_dll, "JVMP_GetPlugin");
  if (!fJVMP_GetPlugin)
    {
	  GetDlErrorText(errorMsg);
      fprintf(stderr, "Cannot load plugin DLL: %s", errorMsg);
	  return JNI_FALSE;
    }	  
  (*fJVMP_GetPlugin)(&g_jvmp_context);
  return JNI_TRUE;
}

static jint initJVM(JavaVM** jvm, JVMP_CallingContext** ctx, 
					JavaVMInitArgs* vm_args)
{
  jint res = 
	(g_jvmp_context->JVMP_GetRunningJVM)(jvm, ctx, vm_args, JNI_FALSE);
  if (res == JNI_TRUE) 
    {
		//fprintf(stderr, "Got JVM!!!\n");
      return JNI_TRUE;
    }
  else 
    {
      fprintf(stderr, "BAD - NO JVM!!!!");
      return JNI_FALSE;
    }  
}

/* this one is called only on Netscape side */

int JVMP_ExecuteShmRequest(JVMP_ShmRequest* req)
{
  int vendor, funcno, id, *pid;
  char sign[40], *buf, *status, *url, *target;
  int  len, argc, *pargc;

  if (!req) return 0;
  vendor = JVMP_GET_VENDOR(req->func_no);
  if (vendor != WFNetscape4VendorID) return 0;
  funcno = JVMP_GET_EXT_FUNCNO(req->func_no);
  switch(funcno)
    {
	case 2:
	  sprintf(sign, "Iia[0]");
	  len = 0; pargc = &argc;
	  JVMP_DecodeRequest(req, 0, sign, &id, &pargc, &buf);
	  pargc = &argc;
	  PutParams(id, &buf, pargc, &len);
	  sprintf(sign, "Iia[%d]", len);
	  JVMP_EncodeRequest(req, sign, id, pargc, buf);
	  free(buf);
	  break;
	case 3:
	  sprintf(sign, "IS");
	  JVMP_DecodeRequest(req, 1, sign, &id, &status);
	  req->retval = ShowStatus(id, status);
	  free(status);
	  break;
	case 4:
	  sprintf(sign, "ISS");
	  JVMP_DecodeRequest(req, 1, sign, &id, &url, &target);
	  req->retval = ShowDocument(id, url, target);
	  free(url); free(target);
	default:
	  return 0;
	}
  return 1;
}

#ifdef XP_UNIX
static void
request_handler(Widget w, XtPointer data, 
		XEvent *raw_evt, Boolean *cont)
{
  JVMP_ShmRequest* req;  
  XPropertyEvent *evt = (XPropertyEvent *) raw_evt;
  if (evt->atom == g_request_handler_atom) 
    {
      while (JVMP_RecvShmRequest(g_data.m_msgid1, 0, &req)) 
		{
		  if (!req) break;
		  JVMP_ExecuteShmRequest(req);
		  JVMP_ConfirmShmRequest(req);
		  JVMP_DeleteShmReq(req);
		  req = NULL;
		}
    }
  //  fprintf(stderr, "leaving request_handler\n");
}

static int AllocateEventWidget(Display* dpy, XID* pxid)
{
  Arg args[40];
  int argc;
  Widget w;
  
  if (!dpy || !pxid) return 0;
  argc = 0;
  XtSetArg(args[argc], XmNallowShellResize, True); argc++;
  XtSetArg(args[argc], XmNwidth, 100); argc++;
  XtSetArg(args[argc], XmNheight, 100); argc++;
  XtSetArg(args[argc], XmNmappedWhenManaged, False); argc++;
  w  = NULL;
  w  = XtAppCreateShell("WF","XApplication",
						vendorShellWidgetClass,
						dpy,
						args,
						argc);
  if (w == NULL) return 0;
  XtRealizeWidget(w);
  XtAddEventHandler(w, PropertyChangeMask, FALSE,
					request_handler, NULL);
  g_request_handler_atom = 
	XInternAtom(dpy, "WF atom", FALSE);
  *pxid = XtWindow(w);
  XFlush(dpy);
  return 1;
}
#endif
NPError
DoDirtyInit()
{
#ifdef XP_UNIX
  JVMP_ShmRequest* req = NULL;
  int* pmsg_id;
  char* npname;
  Dl_info nfo;
  
  g_data.m_msgid = (int)g_ctx->reserved0;
  g_data.m_dpy = NULL;
  NPN_GetValue(NULL, NPNVxDisplay, &(g_data.m_dpy));
  if (!AllocateEventWidget(g_data.m_dpy, &(g_data.m_xid))) 
	{
	  fprintf(stderr, "Cannot allocate event widget\n"); 
	  return NPERR_GENERIC_ERROR;
	}
  req = JVMP_NewShmReq(g_data.m_msgid, 
					   JVMP_NEW_EXT_FUNCNO(WFNetscape4VendorID, 1));
  JVMP_EncodeRequest(req, "Ii", g_data.m_xid, &(g_data.m_msgid1));
  if (!JVMP_SendShmRequest(req, 1)) return NPERR_GENERIC_ERROR;
  JVMP_WaitConfirmShmRequest(req);
  if (!req->retval) return NPERR_GENERIC_ERROR;
  pmsg_id = &(g_data.m_msgid1);
  JVMP_DecodeRequest(req, 0, "Ii", &g_data.m_xid,  &pmsg_id);
  /* just to increment refcount and forbid to unload plugin DLL */ 
  if (dladdr(&DoDirtyInit, &nfo) != 0)
	  LoadDlLibrary(nfo.dli_fname);
  g_inited = 1;
#endif
  return NPERR_NO_ERROR;
}

static jint PutParams(jint id, char** pbuf, int* pargc, int* plen)
{
  PluginInstance* inst;
  int len, argc, i, pos;
  char* buf;
    
  inst = GetInstance(id);
  if (!inst) { 
	fprintf(stderr, "instance %d not found\n", (int)id); 
	*plen = *pargc = 0; *pbuf = NULL;
	return JNI_FALSE; 
  }
  argc = inst->m_argc; len = 0;
  for (i=0; i<argc; i++)
	{
	  len += strlen(inst->m_argn[i]) + 1 + strlen(inst->m_argv[i]) + 1;
	}
  buf = (char*)malloc(len);
  *plen = len;
  pos = 0;
  for (i=0; i<argc; i++)
	{
	  len = strlen(inst->m_argn[i]) + 1;
	  memcpy(buf+pos, inst->m_argn[i], len);
	  pos += len;
	  len = strlen(inst->m_argv[i]) + 1;
	  memcpy(buf+pos, inst->m_argv[i], len);
	  pos += len;
	}
  *pbuf = buf;
  *pargc = argc;
  return JNI_TRUE;
}

static PluginInstance*  GetInstance(jint id)
{
  int i;
  for(i=0; i<MAX_INSTANCES; i++)
	{
	  if (g_instances[i] && (g_instances[i]->m_id == id)) 
		return g_instances[i];
	}
  return NULL;
}

static void SetDocBase(PluginInstance* inst, char* buf, int len)
{
  JVMP_ShmRequest* req;
  if (!inst) return;
  inst->m_docbase = strdup(buf);
}

