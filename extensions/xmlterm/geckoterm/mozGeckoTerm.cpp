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

#include "gtkmozarea.h"
#include "gdksuperwin.h"

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

  // Resize doc shell window
  gSimpleContainer->Resize(alloc->width, alloc->height);

  if (gXMLTerminal) {
    // Resize XMLTerm
    gXMLTerminal->Resize();
  }

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
  GtkWidget *mozArea = NULL;
  GdkSuperWin *superWin = NULL;

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
  result = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                              &mEventQueue);

  if (!mEventQueue) {
    // Create the event queue for the thread
    result = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(result)) {
      NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
      return result;
  }

    // Get the event queue for the thread
    result = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);
    if (NS_FAILED(result)) {
      NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
      return result;
    }
  }

  // Create pref object
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
  gtk_window_set_default_size( GTK_WINDOW(mainWin), 740, 484);
  gtk_window_set_title(GTK_WINDOW(mainWin), "XMLterm");
  
  mozArea = gtk_mozarea_new();
  gtk_container_add(GTK_CONTAINER(mainWin), mozArea);
  gtk_widget_realize(mozArea);
  gtk_widget_show(mozArea);
  superWin = GTK_MOZAREA(mozArea)->superwin;

  gdk_window_show(superWin->bin_window);                                       
  gdk_window_show(superWin->shell_window);

  gtk_widget_show(mainWin);


  // Configure event handler
  gtk_signal_connect_after( GTK_OBJECT(mainWin), "configure_event",
                            GTK_SIGNAL_FUNC(event_processor_configure),
                            mainWin);

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
  GtkAllocation *alloc = &GTK_WIDGET(mainWin)->allocation;
    
  // Initialize container it to hold a doc shell
  result = gSimpleContainer->Init((nsNativeWidget *) superWin,
                              alloc->width, alloc->height, mPref);

  if (NS_FAILED(result)) {
    return result; // Exit main program
  }

  // Get reference to doc shell embedded in a simple container
  nsCOMPtr<nsIDocShell> docShell;
  result = gSimpleContainer->GetDocShell(*getter_AddRefs(docShell));

  if (NS_FAILED(result) || !docShell) {
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
  result = mozXMLTermUtils::ConvertDocShellToDOMWindow(docShell,
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
                             "chrome://communicator/content/xmlterm/xmlterm.html");
  if (NS_FAILED(result))
    return result;
#endif

#if 0
  // Create an XMLTERM and attach to web shell
  result = NS_NewXMLTerminal(getter_AddRefs(gXMLTerminal));

  if(!gXMLTerminal)
    result = NS_ERROR_OUT_OF_MEMORY;

  if (NS_SUCCEEDED(result)) {
    result = gXMLTerminal->Init(docShell, nsnull, nsnull);
  }
#endif

  // Discard reference to web shell
  docShell = nsnull;

  // GTK event loop
  gtk_main();

  NS_IF_RELEASE(mEventQueue);

  return 0;
}
