/*
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

GtkWidget *toplevel_window = 0;
GtkWidget *button = 0;
GtkWidget *vbox = 0;
GtkWidget *gtk_socket = 0;

void
insert_mozilla(gpointer data);

int
main(int argc, char **argv)
{
  gtk_init(&argc, &argv);

  toplevel_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_signal_connect(GTK_OBJECT(toplevel_window), "destroy",
		     (GtkSignalFunc) gtk_exit, NULL);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(toplevel_window), vbox);
  gtk_widget_show(vbox);

  button = gtk_button_new_with_label("Insert Mozilla");

  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(insert_mozilla), NULL);

  gtk_widget_show(toplevel_window);

  gtk_main();

  return 0;
}

void
insert_mozilla(gpointer data)
{
  char buffer[20];
  int pid;

  if (gtk_socket)
    return;

  gtk_socket = gtk_socket_new();
  gtk_box_pack_start(GTK_BOX(vbox), gtk_socket, TRUE, TRUE, 0);
  gtk_widget_show(gtk_socket);

  sprintf(buffer, "%#lx", GDK_WINDOW_XWINDOW(gtk_socket->window));

  gdk_flush();

  if ((pid = fork()) == 0) { /* child */
    execl("./TestGtkEmbedChild", "./TestGtkEmbedChild", buffer, NULL);
    fprintf(stderr, "can't exec child\n");
    _exit(1);
  }
  else if (pid > 0) { /* parent */
  }
  else {
    fprintf(stderr, "Can't fork.\n");
  }
}
