#include <gtk/gtk.h>
#include <gtkmozembed.h>
int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *mozembed;
	GtkWidget *container;
	char *url;

	gtk_init(&argc, &argv);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	
	container = gtk_notebook_new();
	mozembed = gtk_moz_embed_new();
	label = gtk_label_new("Can you see this message?\n"
			      "Once you switch to mozembed page " 
			      "you never see this message.");

	gtk_signal_connect(GTK_OBJECT(mozembed), "destroy",
					 GTK_SIGNAL_FUNC(gtk_main_quit),
NULL);

	gtk_container_add(GTK_CONTAINER(window), container);

	gtk_notebook_append_page(GTK_NOTEBOOK(container),
			label,
			gtk_label_new("gtk label"));

	gtk_notebook_append_page(GTK_NOTEBOOK(container),
			mozembed,
			gtk_label_new("mozembed"));




	gtk_widget_set_usize(window, 400, 300);
	gtk_widget_show(mozembed);
	gtk_widget_show(label);
	gtk_widget_show_all(window);

	url = (argc > 1) ? argv[1] : (char *)"localhost";
	gtk_moz_embed_load_url(GTK_MOZ_EMBED(mozembed), url);

	gtk_main();
	
	return 0;
}
