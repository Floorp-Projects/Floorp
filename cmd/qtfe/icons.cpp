/* $Id: icons.cpp,v 1.1 1998/09/25 18:01:34 ramiro%netscape.com Exp $
 *
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
 * The Initial Developer of this code under the NPL is Arnt
 * Gulbrandsen.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#include "icons.h"

#include <qapp.h>
#include <qdict.h>
#include <qregexp.h>

#include "il_icons.h"

RCSTAG("$Id: icons.cpp,v 1.1 1998/09/25 18:01:34 ramiro%netscape.com Exp $");

// generated file with heaps of icons
#include "icon-pixmaps.inc"

/*
  Extracted wisdom of the ancients, mapping IL_ constants to file names.

  This is generated from xfe/icons.c by a quick-and-dirty Perl
  script and massaged by hand. We put this in the source code
  since we do not want to depend on xfe/ files.
  */
static struct {
    unsigned int         code;
    const char          *name;
} icon_map_vec[] = {
    {IL_IMAGE_DELAYED, "IReplace"},
    {IL_IMAGE_NOT_FOUND, "IconUnknown"},
    {IL_IMAGE_BAD_DATA, "IBad"},
    {IL_IMAGE_EMBED, "IconUnknown"},
    {IL_IMAGE_INSECURE, "SEC_Replace"},
#if 0
    {IL_ICON_DESKTOP_NAVIGATOR, "Desk_Navigator"},
    {IL_ICON_DESKTOP_MSGCENTER, "Desk_MsgCenter"},
    {IL_ICON_DESKTOP_ABOOK, "Desk_Address"},
    {IL_ICON_DESKTOP_BOOKMARK, "Desk_Bookmark"},
    {IL_ICON_DESKTOP_NOMAIL, "Desk_Messenger"},
    {IL_ICON_DESKTOP_YESMAIL, "Desk_NewMail"},
    {IL_ICON_DESKTOP_NEWS, "Desk_Collabra"},
    {IL_ICON_DESKTOP_MSGCOMPOSE, "Desk_MsgCompose"},
    {IL_ICON_DESKTOP_EDITOR, "Desk_Composer"},
    {IL_ICON_DESKTOP_COMMUNICATOR, "Desk_Communicator"},
    {IL_ICON_DESKTOP_HISTORY, "Desk_History"},
    {IL_ICON_DESKTOP_SEARCH, "Desk_Search"},
    {IL_ICON_DESKTOP_JAVACONSOLE, "Desk_JavaConsole"},
#endif
    {IL_GOPHER_TEXT, "GText"},
    {IL_GOPHER_IMAGE, "GImage"},
    {IL_GOPHER_BINARY, "GBinary"},
    {IL_GOPHER_SOUND, "GAudio"},
    {IL_GOPHER_MOVIE, "GMovie"},
    {IL_GOPHER_FOLDER, "GFolder"},
    {IL_GOPHER_SEARCHABLE, "GFind"},
    {IL_GOPHER_TELNET, "GTelnet"},
    {IL_GOPHER_UNKNOWN, "GUnknown"},
#if 0
    {IL_EDITOR_NEW, "ed_new"},
    {IL_EDITOR_NEW_GREY, "ed_new.i"},
    {IL_EDITOR_OPEN, "ed_open"},
    {IL_EDITOR_OPEN_GREY, "ed_open.i"},
    {IL_EDITOR_SAVE, "ed_save"},
    {IL_EDITOR_SAVE_GREY, "ed_save.i"},
    {IL_EDITOR_BROWSE, "ed_browse"},
    {IL_EDITOR_BROWSE_GREY, "ed_browse.i"},
    {IL_EDITOR_CUT, "ed_cut"},
    {IL_EDITOR_CUT_GREY, "ed_cut.i"},
    {IL_EDITOR_COPY, "ed_copy"},
    {IL_EDITOR_COPY_GREY, "ed_copy.i"},
    {IL_EDITOR_PASTE, "ed_paste"},
    {IL_EDITOR_PASTE_GREY, "ed_paste.i"},
    {IL_EDITOR_PRINT, "ed_print"},
    {IL_EDITOR_PRINT_GREY, "ed_print.i"},
    {IL_EDITOR_FIND, "ed_find"},
    {IL_EDITOR_FIND_GREY, "ed_find.i"},
    {IL_EDITOR_PUBLISH, "ed_publish"},
    {IL_EDITOR_PUBLISH_GREY, "ed_publish.i"},
    {IL_ICON_BACK_GREY, "TB_Back.i"},
    {IL_ICON_LOAD_GREY, "TB_LoadImages.i"},
    {IL_ICON_FWD_GREY, "TB_Forward.i"},
    {IL_ICON_STOP_GREY, "TB_Stop.i"},
    {IL_ALIGN4_RAISED, "ImgB2B_r"},
    {IL_ALIGN5_RAISED, "ImgB2D_r"},
    {IL_ALIGN3_RAISED, "ImgC2B_r"},
    {IL_ALIGN2_RAISED, "ImgC2C_r"},
    {IL_ALIGN7_RAISED, "ImgWL_r"},
    {IL_ALIGN6_RAISED, "ImgWR_r"},
    {IL_ALIGN1_RAISED, "ImgT2T_r"},
    {IL_EDITOR_NEW_PT, "ed_new_pt"},
    {IL_EDITOR_OPEN_PT, "ed_open_pt"},
    {IL_EDITOR_SAVE_PT, "ed_save_pt"},
    {IL_EDITOR_BROWSE_PT, "ed_browse_pt"},
    {IL_EDITOR_CUT_PT, "ed_cut_pt"},
    {IL_EDITOR_COPY_PT, "ed_copy_pt"},
    {IL_EDITOR_PASTE_PT, "ed_paste_pt"},
    {IL_EDITOR_PRINT_PT, "ed_print_pt"},
    {IL_EDITOR_FIND_PT, "ed_find_pt"},
    {IL_EDITOR_PUBLISH_PT, "ed_publish_pt"},
    {IL_EDITOR_NEW_PT_GREY, "ed_new_pt.i"},
    {IL_EDITOR_OPEN_PT_GREY, "ed_open_pt.i"},
    {IL_EDITOR_SAVE_PT_GREY, "ed_save_pt.i"},
    {IL_EDITOR_BROWSE_PT_GREY, "ed_browse_pt.i"},
    {IL_EDITOR_CUT_PT_GREY, "ed_cut_pt.i"},
    {IL_EDITOR_COPY_PT_GREY, "ed_copy_pt.i"},
    {IL_EDITOR_PASTE_PT_GREY, "ed_paste_pt.i"},
    {IL_EDITOR_PRINT_PT_GREY, "ed_print_pt.i"},
    {IL_EDITOR_FIND_PT_GREY, "ed_find_pt.i"},
    {IL_EDITOR_PUBLISH_PT_GREY, "ed_publish_pt.i"},
    {IL_ICON_BACK_GREY, "TB_Back.i"},
    {IL_ICON_LOAD_GREY, "TB_LoadImages.i"},
    {IL_ICON_FWD_GREY, "TB_Forward.i"},
    {IL_ICON_STOP_GREY, "TB_Stop.i"},
    {IL_EDITOR_BOLD, "ed_bold"},
    {IL_EDITOR_BULLET, "ed_bullet"},
    {IL_EDITOR_CENTER, "ed_center"},
    {IL_EDITOR_CLEAR, "ed_clear"},
    {IL_EDITOR_COLOR, "ed_color"},
    {IL_EDITOR_FIXED, "ed_fixed"},
    {IL_EDITOR_GROW, "ed_grow"},
    {IL_EDITOR_GROW_GREY, "ed_grow.i"},
    {IL_EDITOR_HRULE, "ed_hrule"},
    {IL_EDITOR_IMAGE, "ed.image"},
    {IL_EDITOR_INDENT, "ed.indent"},
    {IL_EDITOR_ITALIC, "ed.italic"},
    {IL_EDITOR_LEFT, "ed_left"},
    {IL_EDITOR_LINK, "ed_link"},
    {IL_EDITOR_NUMBER, "ed_number"},
    {IL_EDITOR_OUTDENT, "ed_outdent"},
    {IL_EDITOR_PROPS, "ed_props"},
    {IL_EDITOR_RIGHT, "ed_right"},
    {IL_EDITOR_SHRINK, "ed_shrink"},
    {IL_EDITOR_SHRINK_GREY, "ed_shrink.i"},
    {IL_EDITOR_TARGET, "ed_target"},
    {IL_EDITOR_TABLE, "ed_table"},
#endif
    {IL_EDIT_UNSUPPORTED_TAG, "ed_tag"},
    {IL_EDIT_UNSUPPORTED_END_TAG, "ed_tage"},
    {IL_EDIT_FORM_ELEMENT, "ed_form"},
    {IL_EDIT_NAMED_ANCHOR, "ed_target"},
    {IL_SA_SIGNED, "A_Signed"},
    {IL_SA_ENCRYPTED, "A_Encrypt"},
    {IL_SA_NONENCRYPTED, "A_NoEncrypt"},
    {IL_SA_SIGNED_BAD, "A_SignBad"},
    {IL_SA_ENCRYPTED_BAD, "A_EncrypBad"},
    {IL_SMIME_ATTACHED, "M_Attach"},
    {IL_SMIME_SIGNED, "M_Signed"},
    {IL_SMIME_ENCRYPTED, "M_Encrypt"},
    {IL_SMIME_ENC_SIGNED, "M_SignEncyp"},
    {IL_SMIME_SIGNED_BAD, "M_SignBad"},
    {IL_SMIME_ENCRYPTED_BAD, "M_EncrypBad"},
    {IL_SMIME_ENC_SIGNED_BAD, "M_SgnEncypBad"},
    {IL_MSG_ATTACH, "M_ToggleAttach"},
    {0, 0 }
};



static QDict<QIconSet> * icons = 0;
static QDict<QPixmap> * pixmaps = 0;

static const char *map_il_to_filename[IL_MSG_LAST+1];

static void clean()
{
    delete icons;
    icons = 0;
    delete pixmaps;
    pixmaps = 0;
}


static void make()
{
    if ( !icons ) {
	qAddPostRoutine( clean );
	icons = new QDict<QIconSet>( 129 );
	icons->setAutoDelete( TRUE );
	pixmaps = new QDict<QPixmap>( 129 );
	pixmaps->setAutoDelete( TRUE );
	int i = 0;
	while ( icon_map_vec[i].name ) {
	    map_il_to_filename[icon_map_vec[i].code] = icon_map_vec[i].name;
	    i++;
	}
    }
}


static int look( const char * name )
{
    int i = 0;
    QString tmp;
    tmp.sprintf( "/%s.gif$", name );
    QRegExp r( tmp );
    while( embed_vec[i].size && r.match( embed_vec[i].name ) < 0 )
	i++;
    if ( embed_vec[i].size )
	return i;
    return -1;
}

static int find( const char * name )
{
    int i = look( name );
    if ( i < 0 )
	fatal( "could not find icon/movie '%s'", name );
    return i;
}

static QIconSet findIconSet( const char * name )
{
    make();

    QIconSet * s = icons->find( name );
    if ( s )
	return *s;

    int i = find( name );
    QPixmap pm;
    pm.loadFromData( embed_vec[i].data, embed_vec[i].size );
    s = new QIconSet( pm, QIconSet::Large );

    // .i
    QString add;
    add = name;
    add += ".i";
    i = find( add );
    pm.loadFromData( embed_vec[i].data, embed_vec[i].size );
    s->setPixmap( pm, QIconSet::Large, QIconSet::Disabled );

    // .mo
    add = name;
    add += ".mo";
    i = find( add );
    pm.loadFromData( embed_vec[i].data, embed_vec[i].size );
    s->setPixmap( pm, QIconSet::Large, QIconSet::Active );

    icons->insert( name, s );
    return *s;
}


Icon::Icon( const char * name )
    : QIconSet( findIconSet( name ) )
{
    // nothing
}


static QPixmap findPixmap( const char * name )
{
    make();

    QPixmap * pm = pixmaps->find( name );
    if ( pm )
	return *pm;

    int i = look( name );
    if ( i < 0 )
	i = look( "IconUnknown" );

 
    pm = new QPixmap();
    if ( i >= 0 )
	pm->loadFromData( embed_vec[i].data, embed_vec[i].size );
    else
	pm->fill( red );
    pixmaps->insert( name, pm );
    return *pm;
}

static QPixmap findPixmap( int il_id )
{
    make();
    const char *s  = map_il_to_filename[il_id];
    if ( !s )
	s = "IconUnknown.gif";
    return findPixmap( s );
}



Pixmap::Pixmap( const char * name )
    : QPixmap( findPixmap( name ) )
{

}

Pixmap::Pixmap( int il_id )
    : QPixmap( findPixmap( il_id ) )
{

}



static QByteArray * findMovie( const char * name )
{
    int i = find( name );
    QByteArray * a = new QByteArray();
    a->setRawData( (const char *)(embed_vec[i].data), embed_vec[i].size );
    return a;
}


Movie::Movie( const char * name )
    : QMovie( *findMovie( name ) )
{
    // nothing
}
