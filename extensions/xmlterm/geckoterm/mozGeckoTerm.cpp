/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozGeckoTerm.cpp: Stand-alone implementation of XMLterm using GTK
//                   and the Mozilla Layout engine

#include "stdio.h"

#include <gtk/gtk.h>

#if 0 // USE_SUPERWIN
#include "gtkmozarea.h"
#include "gdksuperwin.h"
#endif // USE_SUPERWIN 

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsRepository.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIPref.h"

#include "mozXMLTermUtils.h"
#include "mozISimpleContainer.h"
#include "mozIXMLTerminal.h"
#include "mozIXMLTermStream.h"

#define XMLTERM_DLL   "libxmlterm"MOZ_DLL_SUFFIX

static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID,  NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIPrefIID,              NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID,               NS_PREF_CID);
static NS_DEFINE_IID(kLineTermCID,           MOZLINETERM_CID);
static NS_DEFINE_IID(kXMLTermShellCID,       MOZXMLTERMSHELL_CID);

extern "C" void NS_SetupRegistry();

nsCOMPtr<mozISimpleContainer> gSimpleContainer = nsnull;
nsCOMPtr<mozIXMLTerminal> gXMLTerminal = nsnull;

/** Processes thread events */
static void event_processor_callback(gpointer data,
                                     gint source,
                                     GdkInputCondition condition)
{
  nsIEventQueue *eventQueue = (nsIEventQueue*)data;
  eventQueue->ProcessPendingEvents();
  //fprintf(stderr, "event_processor_callback:\n");
  return;
}


static void event_processor_configure(GtkWidget *window,
                                      GdkEvent  *event,
                                      GtkWidget *termWidget)
{
  GtkAllocation *alloc = &GTK_WIDGET(termWidget)->allocation;

  //fprintf(stderr, "event_processor_configure:\n");

  // Resize web shell window
  gSimpleContainer->Resize(alloc->width, alloc->height);

  return;
}

/** Cleans up and exits */
static gint event_processor_quit(gpointer data,
                                 gint source,
                                 GdkInputCondition condition)
{
  fprintf(stderr, "event_processor_quit:\n");

  if (gXMLTerminal) {
    // Finalize XMLTerm and release owning reference to it
    gXMLTerminal->Finalize();
    gXMLTerminal = nsnull;
  }

  // Delete reference to container
  gSimpleContainer = nsnull;

  gtk_main_quit();
  return TRUE;
}


int main( int argc, char *argv[] )
{
  GtkWidget *mainWin = NULL;
  GtkWidget *vertBox = NULL;
  GtkWidget *horBox = NULL;
  GtkWidget *testButton = NULL;
#if 0 // USE_SUPERWIN
  GdkSuperWin *termWidget = NULL;
  GtkWidget *mozArea;
#else // USE_SUPERWIN 
  GtkWidget *termWidget = NULL;
#endif // !USE_SUPERWIN 

  nsIEventQueue *mEventQueue = nsnull;

  nsresult result = NS_OK;

  // Set up registry
  NS_SetupRegistry();

  // Register XMLTermShell and LineTerm interfaces
  result = nsComponentManager::RegisterComponentLib(kLineTermCID,NULL,NULL,
                                           XMLTERM_DLL, PR_FALSE, PR_FALSE);
  printf("mozGeckoTerm: registered LineTerm, result=0x%x\n", result);

  result = nsComponentManager::RegisterComponentLib(kXMLTermShellCID,NULL,NULL,
                                           XMLTERM_DLL, PR_FALSE, PR_FALSE);
  printf("mozGeckoTerm: registered XMLTermShell, result=0x%x\n", result);

  // Get the event queue service 
  NS_WITH_SERVICE(nsIEventQueueService, eventQService,
                  kEventQueueServiceCID, &result);
  if (NS_FAILED(result)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return result;
  }

  // Get the event queue for the thread.
  result = eventQService->GetThreadEventQueue(PR_GetCurrentThread(),
                                              &mEventQueue);

  if (!mEventQueue) {
    // Create the event queue for the thread
    result = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(result)) {
      NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
      return result;
  }

    // Get the event queue for the thread
    result = eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &mEventQueue);
    if (NS_FAILED(result)) {
      NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
      return result;
    }
  }

  // Creat pref object
  nsCOMPtr<nsIPref> mPref = nsnull;
  result = nsComponentManager::CreateInstance(kPrefCID, NULL, 
                                             kIPrefIID, getter_AddRefs(mPref));
  if (NS_FAILED(result)) {
    NS_ASSERTION("Failed to create nsIPref object", PR_FALSE);
    return result;
  }

  mPref->StartUp();
  mPref->ReadUserPrefs();

  // Initialize GTK library
  gtk_set_locale();

  gtk_init(&argc, &argv);

  gdk_rgb_init();

  mainWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size( GTK_WINDOW(mainWin), 600, 400);
  gtk_window_set_title(GTK_WINDOW(mainWin), "XMLterm");

  // VBox top level
  vertBox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(mainWin), vertBox);
  gtk_widget_show(vertBox);

  // HBox for toolbar
  horBox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vertBox), horBox, FALSE, FALSE, 0);

  testButton = gtk_button_new_with_label("Test");
  gtk_box_pack_start (GTK_BOX (horBox), testButton, FALSE, FALSE, 0);
  gtk_widget_show(testButton);

  gtk_widget_show(horBox);

#if 0 // USE_SUPERWIN
  gtk_window_set_policy(GTK_WINDOW(mainWin), PR_TRUE, PR_TRUE, PR_FALSE);
  mozArea = gtk_mozarea_new();
  gtk_container_add(GTK_CONTAINER(mainWin), mozArea);
  gtk_widget_realize(GTK_WIDGET(mozArea));
  termWidget = GTK_MOZAREA(mozArea)->superwin;

#else // USE_SUPERWIN 

  // XMLterm layout widget
  termWidget = gtk_layout_new(NULL, NULL);
  GTK_WIDGET_SET_FLAGS(termWidget, GTK_CAN_FOCUS);
  gtk_widget_set_app_paintable(termWidget, TRUE);

  gtk_box_pack_start(GTK_BOX(vertBox), termWidget, TRUE, TRUE, 0);
  gtk_widget_show_all(termWidget);

  gtk_widget_show(mainWin);
#endif // !USE_SUPERWIN 

  // Configure event handler
  gtk_signal_connect_after( GTK_OBJECT(mainWin), "configure_event",
                            GTK_SIGNAL_FUNC(event_processor_configure),
                            termWidget);

  // Cleanup and exit when window is deleted
  gtk_signal_connect( GTK_OBJECT(mainWin), "delete_event",
                      GTK_SIGNAL_FUNC(event_processor_quit),
                      NULL);
  
  // Check for input in the events queue file descriptor
  gdk_input_add(mEventQueue->GetEventQueueSelectFD(),
                GDK_INPUT_READ,
                event_processor_callback,
                mEventQueue);

  // Create simple container
  result = NS_NewSimpleContainer(getter_AddRefs(gSimpleContainer));

  if (NS_FAILED(result) || !gSimpleContainer) {
    return result; // Exit main program
  }

  // Determine window dimensions
  GtkAllocation *alloc = &GTK_WIDGET(termWidget)->allocation;
    
  // Initialize container to hold a web shell
  result = gSimpleContainer->Init((nsNativeWidget *) termWidget,
                              alloc->width, alloc->height, mPref);

  if (NS_FAILED(result)) {
    return result; // Exit main program
  }

  // Get reference to web shell embedded in a simple container
  nsCOMPtr<nsIWebShell> webShell;
  result = gSimpleContainer->GetWebShell(*getter_AddRefs(webShell));

  if (NS_FAILED(result) || !webShell) {
    return result; // Exit main program
  }

#if 0
  // TEMPORARY: Testing mozIXMLTermStream
  nsAutoString streamData = "<HTML><HEAD><TITLE>Stream Title</TITLE>"
                            "<SCRIPT language='JavaScript'>"
                            "function clik(){ dump('click\\n');return(false);}"
                            "</SCRIPT></HEAD>"
                            "<BODY><B>Stream Body "
                            "<SPAN STYLE='color: blue' onClick='return clik();'>Clik</SPAN></B>"
                            "</BODY></HTML>";

  nsCOMPtr<mozIXMLTermStream> stream;
  result = NS_NewXMLTermStream(getter_AddRefs(stream));
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozGeckoTerm: Failed to create stream\n");
    return result;
  }

  nsCOMPtr<nsIDOMWindow> outerDOMWindow;
  result = mozXMLTermUtils::ConvertWebShellToDOMWindow(webShell,
                                              getter_AddRefs(outerDOMWindow));

  if (NS_FAILED(result) || !outerDOMWindow)
    return NS_ERROR_FAILURE;

  result = stream->Open(outerDOMWindow, nsnull, "chrome://dummy", "text/html",
                        0);
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozGeckoTerm: Failed to open stream\n");
    return result;
  }

  result = stream->Write(streamData.GetUnicode());
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozGeckoTerm: Failed to write to stream\n");
    return result;
  }

  result = stream->Close();
  if (NS_FAILED(result)) {
    fprintf(stderr, "mozGeckoTerm: Failed to close stream\n");
    return result;
  }
#else
  // Load initial XMLterm document
  result = gSimpleContainer->LoadURL(
                             "chrome://xmlterm/content/xmlterm.html");
  if (NS_FAILED(result))
    return result;
#endif

#if 0
  // Create an XMLTERM and attach to web shell
  result = NS_NewXMLTerminal(getter_AddRefs(gXMLTerminal));

  if(!gXMLTerminal)
    result = NS_ERROR_OUT_OF_MEMORY;

  if (NS_SUCCEEDED(result)) {
    result = gXMLTerminal->Init(webShell, nsnull, nsnull);
  }
#endif

  // Discard reference to web shell
  webShell = nsnull;

  // GTK event loop
  gtk_main();

  NS_IF_RELEASE(mEventQueue);

  return 0;
}
