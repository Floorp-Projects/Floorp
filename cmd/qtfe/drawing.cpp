/* $Id: drawing.cpp,v 1.1 1998/09/25 18:01:33 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include "libi18n.h"
#include "QtContext.h"
#include <qpainter.h>
#include <qdrawutl.h>


/* Taken from the xfe implementation. */

char *
QtContext::translateISOText(int charset, char *ISO_Text)
{
  unsigned char *s;

  /* charsets such as Shift-JIS contain 0240's that are valid */
  if (INTL_CharSetType(charset) != SINGLEBYTE)
    return ISO_Text;

  /* When &nbsp; is encountered, display a normal space character instead.
     This is necessary because the MIT fonts are messed up, and have a
     zero-width character for nobreakspace, so we need to print it as a
     normal space instead. */
  if (ISO_Text)
    for (s = (unsigned char *) ISO_Text; *s; s++)
      if (*s == 0240) *s = ' ';

  return ISO_Text;
}


static QFont getFontWithFamily( const char *fam, int pts, int wgt, bool ita )
{
    QString familyList = fam;
    QString token;
    QString realFamily;

    QFont returnFont( "times", pts, wgt, ita );

    //    debug( "Font family list: %s", fam );
    int pos;
    do {
	pos = familyList.find( "," );
	if ( pos != -1 ) {
	    token = familyList.left( pos );
	    familyList.remove( 0, pos + 1 );
	} else {
	    token = familyList;
	}
	token = token.stripWhiteSpace();
	if ( token[0] == '"' )
	    token.remove( 0, 1 );
	int len = token.length();
	if ( token[len - 1] == '"' )
	    token.remove( len - 1, 1 );
	returnFont.setFamily( token );
	// fi.setFont( returnFont );  Should chech if font family is available
	// debug( "Font family chosen: %s", token.data() );
	return returnFont;
    } while (pos != -1 );
    returnFont.setFamily( "times" );
    return returnFont;	
}




// return a QFont* for a given Netscape TextStruct.
// This will use a cached result if possible
QFont QtContext::getFont(LO_TextAttr *text_attr)
{
  if (text_attr->FE_Data){
    return *((QFont*) (text_attr->FE_Data));
  }

  const char* fam;
  int pts, wgt;
  bool ita;

  if (!text_attr->font_face){
    fam= (text_attr->fontmask & LO_FONT_FIXED) ? "courier" : "times";
  }
  else 
    fam=text_attr->font_face;

  wgt=text_attr->font_weight;
  ita=text_attr->fontmask & LO_FONT_ITALIC;

    //#warning Matthias TODO implement charsets
  //  result->setCharSet(text_attr->charset);
  pts = int(text_attr->point_size);
  if ( pts <= 0) {
    int size3 = 12;
    switch ( text_attr->size ) {
      case 1:
	pts = 8;
	break;
      case 2:
	pts = 10;
        break;
      case 3:
	pts = 12;
        break;
      case 4:
	pts = 14;
        break;
      case 5:
	pts = 18;
        break;
      case 6:
	pts = 24;
        break;
      default:
	pts = (text_attr->size-6)*4+24;
    }
    pts = pts * size3 / 12;
  }

  QFont result = getFontWithFamily(fam, pts, wgt, ita);

  if (!wgt) result.setBold(text_attr->fontmask & LO_FONT_BOLD);
  result.setStrikeOut(text_attr->attrmask & LO_ATTR_STRIKEOUT);
  result.setUnderline(text_attr->attrmask & LO_ATTR_UNDERLINE);
  return result;
}

void  QtContext::releaseTextAttrFeData(LO_TextAttr *attr){
  if (attr->FE_Data){
    delete (QFont*) (attr->FE_Data);
    attr->FE_Data = 0;
  }
  
}


void QtContext::displaySubtext( int /* iLocation  */,
				LO_TextStruct *text, int start_pos, int end_pos,
				bool )
{
  LO_TextAttr *text_attr = text->text_attr;
  QFont font = getFont(text_attr);
  QColor fg(text_attr->fg.red,
	    text_attr->fg.green,
	    text_attr->fg.blue);
  QColor bg(text_attr->bg.red,
	    text_attr->bg.green,
	    text_attr->bg.blue);


  painter()->setPen(fg);
  painter()->setFont(font);
  QColor obg = painter()->backgroundColor();

  bool selected_p = false;

  QFontMetrics fm = painter()->fontMetrics();

  /* this check is inspired by the fxe. not too sure but it cannot really harm....
   */
  if ((text->ele_attrmask & LO_ELE_SELECTED) &&
	(start_pos >= text->sel_start) && (end_pos-1 <= text->sel_end))
    selected_p = true;

  /* use black and white for selections
   */
  if (selected_p){
    painter()->setBackgroundMode(OpaqueMode);
    painter()->setBackgroundColor(black);
    painter()->setPen(white);
  }
  else if (text->text_attr->no_background){
    painter()->setPen(fg);
  }
  else {
    painter()->setBackgroundMode(OpaqueMode);
    painter()->setBackgroundColor(bg);
    painter()->setPen(fg);
  }

  /* do the drawing */
  painter()->drawText(text->x - documentXOffset(),
		      text->y + text->y_offset + fm.ascent() - documentYOffset(),
		      (char*) (text->text + start_pos),
		      end_pos - start_pos);
  
  /* reset the background mode of the qpainter
   */
  painter()->setBackgroundColor(TransparentMode);
  painter()->setBackgroundColor(obg);
}

void QtContext::clearPainter()
{
  if (internal_painter && internal_painter->isActive())
    internal_painter->end();
  if (internal_painter) delete internal_painter;
  internal_painter = 0;
}

int QtContext::documentXOffset() const
{
    return 0;
}

int QtContext::documentYOffset() const
{
    return 0;
}

void QtContext::displayText (int iLocation, LO_TextStruct *text, bool){
  displaySubtext(iLocation, text, 0, text->text_len, false);
}

void QtContext::getTextInfo(LO_TextStruct *text, LO_TextInfo *text_info){
#if defined(XP_WINDAUGE)
  QFontMetrics fontMetrics(QFont("helvetica", 12));   // EE ###getFont(text->text_attr));
#else
  QFontMetrics fontMetrics(getFont(text->text_attr));
#endif
  text_info->max_width = fontMetrics.width((char*)text->text, text->text_len);
  text_info->ascent = fontMetrics.ascent();
  text_info->descent = fontMetrics.descent();
  text_info->lbearing = fontMetrics.minLeftBearing();
  text_info->rbearing = fontMetrics.minRightBearing();
}


void QtContext::getTextFrame(LO_TextStruct *text, int32 start,
			     int32 end, XP_Rect *frame)
{
  QFontMetrics fontMetrics(getFont(text->text_attr));
  frame->left   = text->x + text->x_offset;
  frame->top    = text->y + text->y_offset;
  frame->right  = frame->left + fontMetrics.width( (char*)text->text+start,
						   end-start );
  frame->bottom = frame->top + fontMetrics.lineSpacing();
}




void QtContext::displayTable(int loc, LO_TableStruct *ts){


//   fe_Drawable *fe_drawable = CONTEXT_DATA (context)->drawable;

//   long x = ts->x + ts->x_offset - CONTEXT_DATA (context)->document_x +
//       fe_drawable->x_origin;
//   long y = ts->y + ts->y_offset - CONTEXT_DATA (context)->document_y +
//       fe_drawable->y_origin;

//   if ((x > 0 && x > CONTEXT_DATA (context)->scrolled_width) ||
//       (y > 0 && y > CONTEXT_DATA (context)->scrolled_height) ||
//       (x + ts->width < 0) ||
//       (y + ts->line_height< 0))
//     return;


  //  painter()->drawText(text->x - documentXOffset(),
  //			     text->y + text->y_offset + fm.ascent() - documentYOffset(),
  //		     (char*) (text->text + start_pos),
  //		     end_pos - start_pos);

  long x = ts->x + ts->x_offset - documentXOffset() + getXOrigin();
  long y = ts->y + ts->y_offset - documentYOffset() + getYOrigin();
  
  bool hasBorder = (ts->border_top_width > 0 || ts->border_right_width > 0
		    || ts->border_bottom_width > 0 || ts->border_left_width > 0);

  QRect table_rect(x,y,ts->width,ts->height);


  if (hasBorder)
  {
      int w = ts->border_top_width;
      displayTableBorder(ts, table_rect, w );
  }
}

QColorGroup QtContext::colorGroup(const QColor& col)
{
  QColor topsh = col.light();
  QColor botsh = col.dark();
  int h,s,v;
  col.hsv(&h,&s,&v);
  if ( v > 200 ) {
      topsh.setHsv(h,s,200);
  } else if ( v < 90 ) {
      topsh.setHsv(h,s,90);
      botsh.setHsv(h,s,60);
  }

  return QColorGroup(col,
		contentWidget()->colorGroup().background(),
		topsh,
		botsh,
		col, 
		contentWidget()->colorGroup().text(),
		col);
}

void QtContext::displayTableBorder(LO_TableStruct *ts,
				   QRect r, int bw)
{

  if (!contentWidget())
    return;

  QColor col (ts->border_color.red,
	      ts->border_color.green,
	      ts->border_color.blue);

  QColorGroup g = colorGroup( col );

  switch (ts->border_style)
	{
	case BORDER_NONE:
	  break;

	case BORDER_DOTTED:
	case BORDER_DASHED:
	case BORDER_SOLID:
	  qDrawPlainRect(painter(), r, col, bw); 
	  break;

	case BORDER_DOUBLE:
	  {
	    int ws = (bw+1)/3;
	    qDrawPlainRect(painter(), r, col, ws);
	    qDrawPlainRect(painter(), QRect(r.x()+bw-ws, r.y()+bw-ws,
					    r.width()- 2*(bw-ws),
					    r.height()- 2*(bw-ws)),
			   col, ws);
	  }
	  break;

	case BORDER_GROOVE:
	  if (bw<2) bw = 2;
	  qDrawShadeRect(painter(), r, g, true, 1, bw-2);
	  break;

	case BORDER_RIDGE:
	  if (bw<2) bw = 2;
	  qDrawShadeRect(painter(), r, g, false, 1, bw-2);
	  break;

	case BORDER_INSET:
	  qDrawShadePanel(painter(), r, g, true, bw);
	  break;

	case BORDER_OUTSET:
	  qDrawShadePanel(painter(), r, g, false, bw);
	  break;

	default:
	  XP_ASSERT(0);
	  break;
	}
}


void QtContext::displayBorder(int iLocation, int x, int y, int width,
                  int height, int bw, LO_Color *color, LO_LineStyle style)
{
//     debug( "QTFE_DisplayBorder %d, %d, %d, %d, %d, %d, %p, %d",
// 	    iLocation, x, y, width,
// 	    height, bw, color, (int)style);

    if (bw <= 0)
      return;

    QColor col (color->red,
		color->green,
		color->blue);

    QColorGroup g = colorGroup( col );

    QRect r(x - documentXOffset() + getXOrigin(),
	    y - documentYOffset() + getYOrigin(),
	    width, 
	    height);
    switch (style){
    case LO_SOLID:
      qDrawPlainRect(painter(), r, col, bw); 
      break;
    case LO_BEVEL:
      if (bw<2)
	bw = 2;
      qDrawShadeRect(painter(), r, g, false, 1, bw-2);
      break;
    default:
      break;
    }
}

/* From ./lay.c: */
void QtContext::displayCell(int loc, LO_CellStruct *cell)
{
    int bw = cell->border_width;
    if (bw <= 0)
      return;
  
    QRect r(cell->x + cell->x_offset - documentXOffset() + getXOrigin(),
	    cell->y + cell->y_offset - documentYOffset() + getYOrigin(),
	    cell->width, 
	    cell->height);
    qDrawShadePanel(painter(), r, 
		    contentWidget()->colorGroup(), 
		    true, bw);
    
}

/* From ./lay.c: */
void QtContext::eraseBackground(int iLocation, int32 x, int32 y,
                    uint32 width, uint32 height, LO_Color *bg)
{
  QColor col (bg->red,
	      bg->green,
	      bg->blue);
  QRect r(x - documentXOffset(),
	  y - documentYOffset(),
	  width, 
	  height);

  painter()->fillRect(r, col);
}

/* From ./lay.c: */
void QtContext::displayHR(int iLocation, LO_HorizRuleStruct *hr)
{
    QPoint p1( hr->x + hr->x_offset - documentXOffset() + getXOrigin(),
	       hr->y + hr->y_offset - documentYOffset() + getYOrigin() );
    QPoint p2 = p1 + QPoint(hr->width,0);
    if ( hr->ele_attrmask & LO_ELE_SHADED ) {
	qDrawShadeLine(painter(), p1, p2, contentWidget()->colorGroup(),
	    hr->height, 1);
    } else {
	QColor hrCol = painter()->pen().color(); //#####???
	painter()->setPen( QPen( hrCol, hr->height) );
	painter()->drawLine( p1, p2 );
    }
//     debug( "QTFE_DisplayHR %d, %p", iLocation, hr);
}

/* From ./lay.c: */
void QtContext::displayBullet(int iLocation, LO_BulletStruct *bul)
{
    
    //The front end should only ever see
    //bullets of type BULLET_ROUND and BULLET_SQUARE

    QCOORD x = bul->x + bul->x_offset - documentXOffset() + getXOrigin();
    QCOORD y = bul->y + bul->y_offset - documentYOffset() + getYOrigin();
    QCOORD w = bul->bullet_size; 


  LO_TextAttr *text_attr = bul->text_attr;
  QColor fg(text_attr->fg.red,
	    text_attr->fg.green,
	    text_attr->fg.blue);
  QColor bg(text_attr->bg.red,
	    text_attr->bg.green,
	    text_attr->bg.blue);

    if ( bul->bullet_type == BULLET_SQUARE ) {
	painter()->fillRect( x, y, w, w, fg );
    } else {
	if ( bul->bullet_type != BULLET_ROUND 
	     && bul->bullet_type != BULLET_BASIC )
	    warning( "QtContext::displayBullet unknown type %d",
		     bul->bullet_type);
	painter()->setBrush( fg ); 
	painter()->setPen( fg ); 
	painter()->drawEllipse( x, y, w, w);
    } 
}
