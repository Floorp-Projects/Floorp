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
 * $Id: gtktest.c,v 1.2 2001/07/12 19:57:39 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void destroy( GtkWidget *widget,
              gpointer   data )
{
    gtk_main_quit();
}

void my_exit() {
 static int t=0;
 fprintf(stderr, "My at exit called %d\n", t++);
}

int g_argc;
char** g_argv;


void* thread_func(void* arg) {
  GtkWidget* window; 
  char* name = (char*) arg;
  GtkWidget* button;
  static int s = 0;
  
  atexit(&my_exit);
  if (s) {
    return;
    while (1) {usleep(100);}
    putenv("DISPLAY=hercules:1");
  };
  s = 1;
  
  /*  gtk_init(&g_argc, &g_argv); */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (destroy), NULL);
    
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    
  button = gtk_button_new_with_label (name);
  /*  
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (destroy), NULL);
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                               GTK_SIGNAL_FUNC (gtk_widget_destroy),
                               GTK_OBJECT (window));
  */
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                             GTK_SIGNAL_FUNC (gtk_main_quit),
                             GTK_OBJECT (window));
  
  
  gtk_container_add (GTK_CONTAINER (window), button);
  gtk_widget_show (button);    
  gtk_widget_show (window);
    
  gtk_main ();
  return NULL;
}


int main(int argc, char** argv) {
  pthread_t t[2];
  void* res;
  
  g_argc = argc;
  g_argv = argv;
  gtk_init(&g_argc, &g_argv);
  atexit(&my_exit);
  
  if (pthread_create(&(t[0]), NULL,  thread_func, "Frame one") != 0) {
    fprintf(stderr, "Cannot create first thread\n");
    exit(1);
  };
  if (1 && pthread_create(&(t[1]), NULL,  thread_func, "Frame two") != 0) {
    fprintf(stderr, "Cannot create second thread\n");
    exit(1);
  };  
  pthread_join(t[0], &res);
  pthread_join(t[1], &res);
  /*while (1) {
    usleep(100);
    fprintf(stderr, ".");
  };
  */
  return 0;
}
