/** simplemdi.c **/
/*
 * Sample code from "GNOME/GTK+ Programming Bible" by Arthur Griffith
 * Modified by Kevin Gibbs (kgibbs@stanford.edu) to provide sample of
 * GtkMozEmbed realization/unrealization crashes.
 *
 * To get a fatal crash, simply run the program, click on the "Mozilla"
 * tab to display the MozEmbed widget, and then try to drag that MDI tab
 * off to a new window.
 * 
 * Although this test might seem elaborate, it is really only a convenient
 * to create a situation where the widget is realized, unrealized, and
 * realized again at some point. (Dragging the MDI tab off to a new window
 * causes all widgets to be unrealized in the old window, and then realized
 * again in the new window.) 
 */

#include <gnome.h>
#include "gtkmozembed.h"

// Testing flags
//

// Define this flag to have the test program use the gtkmozembed widget.
// Without this flag, mozilla is never loaded in the program, and a simple
// widget is used in place of the mozembed widget.
#define USE_MOZILLA_TEST

// Define this flag to have a simpler test than the usual one. The normal
// test builds a notebook inside of each MDI view, with one page being a label
// and the other page being a browser widget. The simpler test does not
// build a notebook and simply puts the browser widget in the MDI view itself.
// Currently, this test is not very interesting, since for some reason all
// the webshells just create and destroy themselves. (???)
//#define SIMPLER_TEST

// Define this flag to use a GnomePixmap instead of a GtkLabel as the
// replacement for the gtkmozembed widget when USE_MOZILLA_TEST is
// undefined. This is to stress test the program a bit more by providing a
// replacement widget slightly more complex than GtkLabel.
// (Has no effect when USE_MOZILLA_TEST is defined.)
#define SAMPLE_PIXMAP


gint eventDelete(GtkWidget *widget,
        GdkEvent *event,gpointer data);
gint eventDestroy(GtkWidget *widget,
        GdkEvent *event,gpointer data);

static void addChild(GtkObject *mdi,gchar *name);
static GtkWidget *setLabel(GnomeMDIChild *child,
        GtkWidget *currentLabel,gpointer data);
static GtkWidget *createView(GnomeMDIChild *child,
        gpointer data);

int main(int argc,char *argv[])
{
    GtkObject *mdi;

    gnome_init("simplemdi","1.0",argc,argv);
    mdi = gnome_mdi_new("simplemdi","Simple MDI");
    gtk_signal_connect(mdi,"destroy",
            GTK_SIGNAL_FUNC(eventDestroy),NULL);

    addChild(mdi,"First");
    addChild(mdi,"Second");
    addChild(mdi,"Third");
    addChild(mdi,"Last");

    gnome_mdi_set_mode(GNOME_MDI(mdi),GNOME_MDI_NOTEBOOK);
    //gnome_mdi_open_toplevel(GNOME_MDI(mdi));

    gtk_main();
    exit(0);
}
static void addChild(GtkObject *mdi,gchar *name)
{
    GnomeMDIGenericChild *child;

    child = gnome_mdi_generic_child_new(name);
    gnome_mdi_add_child(GNOME_MDI(mdi),
            GNOME_MDI_CHILD(child));

    gnome_mdi_generic_child_set_view_creator(child,
            createView,name);
    gnome_mdi_generic_child_set_label_func(child,setLabel,
            NULL);
    gnome_mdi_add_view(GNOME_MDI(mdi),
            GNOME_MDI_CHILD(child));
}
static GtkWidget *createView(GnomeMDIChild *child,
        gpointer data)
{
#ifdef USE_MOZILLA_TEST
    GtkWidget *browser = gtk_moz_embed_new();
#else
#ifndef SAMPLE_PIXMAP
    GtkWidget *browser = gtk_label_new("lynx 0.01a");
#else
    /* Another example -- */
    GtkWidget *browser =
      gnome_pixmap_new_from_file("/usr/share/pixmaps/emacs.png");
#endif /* SAMPLE_PIXMAP */
#endif /* USE_MOZILLA_TEST */

    GtkWidget *notebook = gtk_notebook_new();
    char str[80];

    sprintf(str,"View of the\n%s widget",(gchar *)data);

#ifdef USE_MOZILLA_TEST
    gtk_moz_embed_load_url(GTK_MOZ_EMBED(browser), "http://www.mozilla.org");
#endif /* USE_MOZILLA_TEST */

#ifndef SIMPLER_TEST
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gtk_label_new(str),
			     gtk_label_new("Label"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), browser,
			     gtk_label_new("Mozilla"));
    gtk_widget_show_all(notebook);
    return (notebook);
#else
    gtk_widget_show(browser);
    return (browser);
#endif /* SIMPLER_TEST */
}

static GtkWidget *setLabel(GnomeMDIChild *child,
        GtkWidget *currentLabel,gpointer data)
{
    if(currentLabel == NULL)
        return(gtk_label_new(child->name));

    gtk_label_set_text(GTK_LABEL(currentLabel),
            child->name);
    return(currentLabel);
}
gint eventDestroy(GtkWidget *widget,
        GdkEvent *event,gpointer data) {
    gtk_main_quit();
    return(0);
}
