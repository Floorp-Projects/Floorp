#include <gtk/gtk.h>
#include <gtkmozembed.h>
#include <stdio.h>
#include "nsStringAPI.h"
#include "gtkmozembed_glue.cpp"

int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *mozembed;
	GtkWidget *container;
	char *url;

	gtk_init(&argc, &argv);
	
        static const GREVersionRange greVersion = {
                "1.9a", PR_TRUE,
                "2", PR_TRUE
        };

        char xpcomPath[PATH_MAX];

        nsresult rv =
                GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0,
                                             xpcomPath, sizeof(xpcomPath));
        if (NS_FAILED(rv)) {
                fprintf(stderr, "Couldn't find a compatible GRE.\n");
                return 1;
        }

        rv = XPCOMGlueStartup(xpcomPath);
        if (NS_FAILED(rv)) {
                fprintf(stderr, "Couldn't start XPCOM.");
                return 1;
        }

        rv = GTKEmbedGlueStartup();
        if (NS_FAILED(rv)) {
                fprintf(stderr, "Couldn't find GTKMozEmbed symbols.");
                return 1;
        }

        char *lastSlash = strrchr(xpcomPath, '/');
        if (lastSlash)
                *lastSlash = '\0';

        gtk_moz_embed_set_path(xpcomPath);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	
	container = gtk_notebook_new();
	mozembed = gtk_moz_embed_new();
	label = gtk_label_new("Can you see this message?\n"
			      "Once you switch to mozembed page " 
			      "you never see this message.");

	g_signal_connect(GTK_OBJECT(mozembed), "destroy",
	                 G_CALLBACK(gtk_main_quit),
NULL);

	gtk_container_add(GTK_CONTAINER(window), container);

	gtk_notebook_append_page(GTK_NOTEBOOK(container),
			label,
			gtk_label_new("gtk label"));

	gtk_notebook_append_page(GTK_NOTEBOOK(container),
			mozembed,
			gtk_label_new("mozembed"));




	gtk_widget_set_size_request(window, 400, 300);
	gtk_widget_show(mozembed);
	gtk_widget_show(label);
	gtk_widget_show_all(window);

	url = (argc > 1) ? argv[1] : (char *)"localhost";
	gtk_moz_embed_load_url(GTK_MOZ_EMBED(mozembed), url);

	gtk_main();
	
	return 0;
}
