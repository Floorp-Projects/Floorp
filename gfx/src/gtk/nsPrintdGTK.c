/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Original Code: Syd Logan (syd@netscape.com) 3/12/99 */

#include <gtk/gtk.h>

#include "nspr.h"

#include "nsPrintdGTK.h"

/* A structure to hold widgets that need to be referenced in callbacks. We
   declare this statically in the caller entry point (as a field of 
   UnixPrOps, below), and pass a pointer to the structure to Gtk+ for each 
   signal handler registered. This avoids use of globals. */

typedef struct prwidgets {
	GtkWidget *toplevel;		/* should be set to toplevel window */
	GtkWidget *prDialog;		
	GtkWidget *cmdEntry;
	GtkWidget *pathEntry;
	GtkWidget *browseButton;
	GtkWidget *fpfToggle;
	GtkWidget *greyToggle;
	GtkWidget *letterToggle;
	GtkWidget *legalToggle;
	GtkWidget *execToggle;
	GtkWidget *topSpinner;
	GtkWidget *bottomSpinner;
	GtkWidget *leftSpinner;
	GtkWidget *rightSpinner;
	GtkFileSelection *fsWidget;
} PrWidgets;

typedef struct unixprops {
	UnixPrData *prData;	/* pointer to caller struct */
	PrWidgets widgets;	 
} UnixPrOps;

/* user clicked cancel. tear things down, and set cancel field in 
   caller data to PR_TRUE so printing will not happen */

static void
CancelPrint (GtkWidget *widget, UnixPrOps *prOps)
{
	gtk_main_quit();
	gtk_widget_destroy( GTK_WIDGET(prOps->widgets.prDialog) );
	prOps->prData->cancel = PR_TRUE;
}

/* user selected the Print button. Collect any remaining data from the
   widgets, and set cancel field in caller data to PR_FALSE so printing
   will be performed. Also, tear down dialog and exit inner main loop */
 
static void
DoPrint (GtkWidget *widget, UnixPrOps *prOps)
{
	strcpy( prOps->prData->command, 
		gtk_entry_get_text( GTK_ENTRY( prOps->widgets.cmdEntry ) ) );
	strcpy( prOps->prData->path, 
		gtk_entry_get_text( GTK_ENTRY( prOps->widgets.pathEntry ) ) );

	if ( GTK_TOGGLE_BUTTON( prOps->widgets.fpfToggle )->active == PR_TRUE )
		prOps->prData->fpf = PR_TRUE;
	else
		prOps->prData->fpf = PR_FALSE;

	if ( GTK_TOGGLE_BUTTON( prOps->widgets.greyToggle )->active == PR_TRUE )
		prOps->prData->grayscale = PR_TRUE;
	else
		prOps->prData->grayscale = PR_FALSE;
	if ( GTK_TOGGLE_BUTTON( prOps->widgets.letterToggle )->active == PR_TRUE )
		prOps->prData->size = NS_LETTER_SIZE;
	else if ( GTK_TOGGLE_BUTTON( prOps->widgets.legalToggle )->active == PR_TRUE )
		prOps->prData->size = NS_LEGAL_SIZE;
	else if ( GTK_TOGGLE_BUTTON( prOps->widgets.execToggle )->active == PR_TRUE )
		prOps->prData->size = NS_EXECUTIVE_SIZE;
	else
		prOps->prData->size = NS_A4_SIZE;

	/* margins */

	prOps->prData->top = gtk_spin_button_get_value_as_float( 
		GTK_SPIN_BUTTON(prOps->widgets.topSpinner) );
	prOps->prData->bottom = gtk_spin_button_get_value_as_float( 
		GTK_SPIN_BUTTON(prOps->widgets.bottomSpinner) );
	prOps->prData->left = gtk_spin_button_get_value_as_float( 
		GTK_SPIN_BUTTON(prOps->widgets.leftSpinner) );
	prOps->prData->right = gtk_spin_button_get_value_as_float( 
		GTK_SPIN_BUTTON(prOps->widgets.rightSpinner) );

	/* we got everything... bring down the dialog and tell caller
	   it's o.k. to print */

	gtk_main_quit();
	gtk_widget_destroy( GTK_WIDGET(prOps->widgets.prDialog) );
	prOps->prData->cancel = PR_FALSE;
}

/* User hit ok in file selection widget brought up by the browse button.
   snarf the selected file, stuff it in caller data */

static void
ModifyPrPath (GtkWidget *widget, UnixPrOps *prOps)
{
	strcpy( prOps->prData->path, 
		gtk_file_selection_get_filename(prOps->widgets.fsWidget) );
      	gtk_entry_set_text (GTK_ENTRY (prOps->widgets.pathEntry), 
		prOps->prData->path);
	gtk_widget_destroy( GTK_WIDGET(prOps->widgets.fsWidget) );
}

/* user selected print to printer. de-sensitize print to file fields */

static void
SwitchToPrinter (GtkWidget *widget, UnixPrOps *prOps)
{
	gtk_widget_set_sensitive( prOps->widgets.cmdEntry, PR_TRUE );
	gtk_widget_set_sensitive( prOps->widgets.pathEntry, PR_FALSE );
	gtk_widget_set_sensitive( prOps->widgets.browseButton, PR_FALSE );
	prOps->prData->toPrinter = PR_TRUE;
}

/* user selected print to file. de-sensitize print to printer fields */

static void
SwitchToFile (GtkWidget *widget, UnixPrOps *prOps)
{
	gtk_widget_set_sensitive( prOps->widgets.cmdEntry, PR_FALSE );
	gtk_widget_set_sensitive( prOps->widgets.pathEntry, PR_TRUE );
	gtk_widget_set_sensitive( prOps->widgets.browseButton, PR_TRUE );
	prOps->prData->toPrinter = PR_FALSE;
}

/* user hit the browse button. Pop up a file selection widget and grab
   result */

static void
GetPrPath (GtkWidget *widget, UnixPrOps *prOps)
{
    GtkWidget *fs;

    fs = gtk_file_selection_new("Netscape: File Browser");

    gtk_file_selection_set_filename( GTK_FILE_SELECTION(fs), 
	prOps->prData->path );

    gtk_window_set_modal (GTK_WINDOW(fs),PR_TRUE);

#if 0
    /* XXX not sure what the toplevel window should be. */

    gtk_window_set_transient_for (GTK_WINDOW (fs), 
	GTK_WINDOW (prOps->widgets.toplevel));
#endif

    prOps->widgets.fsWidget = GTK_FILE_SELECTION(fs);

    gtk_signal_connect (GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button), 
	"clicked", GTK_SIGNAL_FUNC(ModifyPrPath), prOps);

    gtk_signal_connect_object (GTK_OBJECT(
	GTK_FILE_SELECTION(fs)->cancel_button), "clicked",
	GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT (fs));

    gtk_widget_show(fs);
}

/* create file data dialog. XXX widget should be NULL */

static void
DoPrintGTK (GtkWidget *widget, UnixPrOps *prOps)
{
	GtkWidget *separator, *dialog, *label, *vbox, *entry, *hbox, 
		*button, *fileButton, *prButton, *table, *spinner1;
        GtkAdjustment *adj;

	prOps->widgets.prDialog = dialog = 
		gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_modal( GTK_WINDOW(dialog), PR_TRUE );
#if 0
	/* not yet sure what the toplevel window should be */

        gtk_window_set_transient_for (GTK_WINDOW (dialog), 
		GTK_WINDOW (prOps->widgets.toplevel));
#endif
      	gtk_window_set_title( GTK_WINDOW(dialog), "Netscape: Print" );

	vbox = gtk_vbox_new (PR_FALSE, 0);
        gtk_container_add (GTK_CONTAINER (dialog), vbox);

	table = gtk_table_new (3, 3, PR_FALSE);
      	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
        gtk_table_set_col_spacings (GTK_TABLE (table), 5);
        gtk_container_set_border_width (GTK_CONTAINER (table), 10);
        gtk_box_pack_start (GTK_BOX (vbox), table, PR_TRUE, PR_TRUE, 5);
	label = gtk_label_new( "Print To:" );
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
        	0, GTK_EXPAND | GTK_FILL, 0, 0);
	button = prButton = gtk_radio_button_new_with_label (NULL, "Printer");
        if ( prOps->prData->toPrinter == PR_TRUE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 0, 1,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	button = fileButton = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "File");
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
        if ( prOps->prData->toPrinter == PR_FALSE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);

	label = gtk_label_new( "Print Command:" );
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
        	0, GTK_EXPAND | GTK_FILL, 0, 0);
	entry = gtk_entry_new ();
      	gtk_entry_set_text (GTK_ENTRY (entry), prOps->prData->command);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 3, 1, 2,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	if ( prOps->prData->toPrinter == PR_FALSE )
		gtk_widget_set_sensitive( entry, PR_FALSE );
	prOps->widgets.cmdEntry = entry;

	label = gtk_label_new( "File Name:" );
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
        	0, GTK_EXPAND | GTK_FILL, 0, 0);
	entry = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 2, 3,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      	gtk_entry_set_text (GTK_ENTRY (entry), prOps->prData->path);
	if ( prOps->prData->toPrinter == PR_TRUE )
		gtk_widget_set_sensitive( entry, PR_FALSE );
	prOps->widgets.pathEntry = entry;

	button = gtk_button_new_with_label ("Browse...");
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 2, 3,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      	gtk_signal_connect (GTK_OBJECT (button), "clicked",
        	GTK_SIGNAL_FUNC (GetPrPath), prOps);
	if ( prOps->prData->toPrinter == PR_TRUE )
		gtk_widget_set_sensitive( entry, PR_FALSE );
	prOps->widgets.browseButton = button;

	separator = gtk_hseparator_new ();
      	gtk_box_pack_start (GTK_BOX (vbox), separator, PR_TRUE, PR_FALSE, 0);

	table = gtk_table_new (2, 4, PR_FALSE);
      	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
        gtk_table_set_col_spacings (GTK_TABLE (table), 5);
        gtk_container_set_border_width (GTK_CONTAINER (table), 10);
      	gtk_box_pack_start (GTK_BOX (vbox), table, PR_TRUE, PR_FALSE, 0);

	label = gtk_label_new( "Print: " );
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	button = gtk_radio_button_new_with_label (NULL, "First Page First");
        if ( prOps->prData->fpf == PR_TRUE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	prOps->widgets.fpfToggle = button;
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 0, 1,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)), 
		"Last Page First");
        if ( prOps->prData->fpf == PR_FALSE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	label = gtk_label_new( "Print: " );
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	button = gtk_radio_button_new_with_label (NULL, "Greyscale");
	prOps->widgets.greyToggle = button;
        if ( prOps->prData->grayscale == PR_TRUE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 2, 3,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)), 
		"Color");
        if ( prOps->prData->grayscale == PR_FALSE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 2, 3,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	label = gtk_label_new( "Paper Size: " );
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	button = gtk_radio_button_new_with_label (NULL, 
		"Letter (8 1/2 x 11 in.)");
	prOps->widgets.letterToggle = button;
        if ( prOps->prData->size == NS_LETTER_SIZE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 3, 4,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)), 
		"Legal (8 1/2 x 14 in.)");
	prOps->widgets.legalToggle = button;
        if ( prOps->prData->size == NS_LEGAL_SIZE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 3, 4,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)), 
		"Executive (7 1/2 x 10 in.)");
	prOps->widgets.execToggle = button;
        if ( prOps->prData->size == NS_EXECUTIVE_SIZE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 1, 2, 4, 5,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	button = gtk_radio_button_new_with_label (
		gtk_radio_button_group (GTK_RADIO_BUTTON (button)), 
		"A4 (210 x 297 mm)");
        if ( prOps->prData->size == NS_A4_SIZE ) 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), 
		PR_TRUE);
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 4, 5,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	/* margins */

	separator = gtk_hseparator_new ();
      	gtk_box_pack_start (GTK_BOX (vbox), separator, PR_TRUE, PR_FALSE, 0);

	hbox = gtk_hbox_new (PR_FALSE, 0);
      	gtk_box_pack_start (GTK_BOX (vbox), hbox, PR_FALSE, PR_FALSE, 5);
	label = gtk_label_new( "Margins (inches):" );
      	gtk_box_pack_start (GTK_BOX (hbox), label, PR_FALSE, PR_FALSE, 10);

	table = gtk_table_new (1, 2, PR_FALSE);
      	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
        gtk_table_set_col_spacings (GTK_TABLE (table), 5);
        gtk_container_set_border_width (GTK_CONTAINER (table), 10);
      	gtk_box_pack_start (GTK_BOX (vbox), table, PR_TRUE, PR_FALSE, 0);
	hbox = gtk_hbox_new (PR_FALSE, 0);
	gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 0, 1,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	label = gtk_label_new( "Top: " );
      	gtk_box_pack_start (GTK_BOX (hbox), label, PR_TRUE, PR_FALSE, 0);
        adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 0.0, 999.0,
        	0.25, 1.0, 0.0);
        prOps->widgets.topSpinner = spinner1 = 
		gtk_spin_button_new (adj, 1.0, 2);
        gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
        gtk_widget_set_usize (spinner1, 60, 0);
        gtk_box_pack_start (GTK_BOX (hbox), spinner1, FALSE, TRUE, 0);

	label = gtk_label_new( "Bottom: " );
      	gtk_box_pack_start (GTK_BOX (hbox), label, PR_TRUE, PR_FALSE, 0);
        adj = (GtkAdjustment *) gtk_adjustment_new (1.0, 0.0, 999.0,
        	0.25, 1.0, 0.0);
        prOps->widgets.bottomSpinner = spinner1 = 
		gtk_spin_button_new (adj, 1.0, 2);
        gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
        gtk_widget_set_usize (spinner1, 60, 0);
        gtk_box_pack_start (GTK_BOX (hbox), spinner1, FALSE, TRUE, 0);
	
	hbox = gtk_hbox_new (PR_FALSE, 0);
	gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 0, 1,
        	GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	label = gtk_label_new( "Left: " );
      	gtk_box_pack_start (GTK_BOX (hbox), label, PR_TRUE, PR_FALSE, 0);
        adj = (GtkAdjustment *) gtk_adjustment_new (0.75, 0.0, 999.0,
        	0.25, 1.0, 0.0);
        prOps->widgets.leftSpinner = spinner1 = 
		gtk_spin_button_new (adj, 1.0, 2);
        gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
        gtk_widget_set_usize (spinner1, 60, 0);
        gtk_box_pack_start (GTK_BOX (hbox), spinner1, FALSE, TRUE, 0);

	label = gtk_label_new( "Right: " );
      	gtk_box_pack_start (GTK_BOX (hbox), label, PR_TRUE, PR_FALSE, 0);
        adj = (GtkAdjustment *) gtk_adjustment_new (0.75, 0.0, 999.0,
        	0.25, 1.0, 0.0);
        prOps->widgets.rightSpinner = spinner1 = 
		gtk_spin_button_new (adj, 1.0, 2);
        gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner1), TRUE);
        gtk_widget_set_usize (spinner1, 60, 0);
        gtk_box_pack_start (GTK_BOX (hbox), spinner1, FALSE, TRUE, 0);

	separator = gtk_hseparator_new ();
      	gtk_box_pack_start (GTK_BOX (vbox), separator, PR_TRUE, PR_FALSE, 0);

        hbox = gtk_hbox_new (PR_FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, PR_TRUE, PR_FALSE, 5);
	button = gtk_button_new_with_label ("Print");
      	gtk_signal_connect (GTK_OBJECT (button), "clicked",
        	GTK_SIGNAL_FUNC (DoPrint), prOps );
        gtk_box_pack_start (GTK_BOX (hbox), button, PR_TRUE, PR_FALSE, 0);

	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default (button);

	button = gtk_button_new_with_label ("Cancel");
      	gtk_signal_connect (GTK_OBJECT (button), "clicked",
        	GTK_SIGNAL_FUNC ( CancelPrint ), prOps);
        gtk_box_pack_start (GTK_BOX (hbox), button, PR_TRUE, PR_FALSE, 0);

	/* Do this here, otherwise, upon creation the callbacks will be
           triggered and the widgets these callbacks set sensitivity on
	   do not exist yet */

      	gtk_signal_connect (GTK_OBJECT (prButton), "clicked",
        	GTK_SIGNAL_FUNC (SwitchToPrinter), prOps);
      	gtk_signal_connect (GTK_OBJECT (fileButton), "clicked",
        	GTK_SIGNAL_FUNC (SwitchToFile), prOps);

	gtk_widget_show_all( dialog );
	gtk_main ();
}

/* public interface to print dialog. Caller passes in preferences using
   argument, we return any changes and indication of whether to print
   or cancel. */

void
UnixPrDialog( UnixPrData *prData )
{
	static UnixPrOps prOps;

	prOps.prData = prData;
	DoPrintGTK( (GtkWidget *) NULL, &prOps );
}

