/* -*- Mode: C++; tab-width: 4 -*-
   BrowserView.cpp -- class definition for the browser view  class
   Created: Radha Kulkarni <radha@netscape.com>, 23-Feb-1998
 */

/* Insert copyright and license here 1998 */

#include "BrowserView.h"
#include <Xfe/XfeAll.h>

#if DEBUG_radha
#define D(x) x
#else
#define D(x)
#endif

XFE_BrowserView::XFE_BrowserView(XFE_Component * toplevel_component,
				 Widget parent,
				 XFE_View * parent_view,
				 MWContext * context)
  : XFE_View(toplevel_component, parent_view, context)
{

  /* create the pane that holds the NavCenter and the HTML views */
  base_widget = XtVaCreateManagedWidget("browserviewBaseWidget", 
										xmFormWidgetClass,
										parent, 
									    NULL);
  ncview = (XFE_NavCenterView *) 0;
  NavCenterShown = False;
  /* create the NavCenter View */
  /*     ncview = new XFE_NavCenterView(toplevel_component, base_widget, this, context);*/

  /* create the HTML view */
  htmlview = new XFE_HTMLView(toplevel_component, base_widget, this, context);

  /* Add ncview and htmlview to the sub-view list of browser view */
  /*    addView(ncview);  */
  addView(htmlview);

  XtManageChild(base_widget);
  /* show the views */
  /*  ncview->show(); */
  htmlview->show();

  /* Attach the pane to the viewparent of the frame */
  XtVaSetValues(base_widget,
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);
  /*
  XtVaSetValues(ncview->getBaseWidget(),
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_NONE,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNresizable,  False,
	    XmNwidth, 300,
		NULL);
*/
  XtVaSetValues(htmlview->getBaseWidget(),
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);
//    showNavCenter();
  setBaseWidget(base_widget);
}

#ifdef UNDEF
Boolean XFE_BrowserView::isCommandEnabled(CommandType  cmd,  void * calldata, XFE_CommandInfo * i) 
{


}

Boolean XFE_BrowserView::handlesCommand(CommandType cmd, void *calldata = NULL,
				 XFE_CommandInfo* i = NULL)
{


}

char* XFE_BrowserView::commandToString(CommandType cmd, void *calldata = NULL,
				 XFE_CommandInfo* i = NULL)
{
}

XP_Bool XFE_BrowserView::isCommandSelected(CommandType cmd, void *calldata = NULL,
			     	 XFE_CommandInfo* = NULL)
{
}
#endif

void
XFE_BrowserView::showNavCenter()
{

  if (!ncview)
	{
	  ncview = new XFE_NavCenterView(m_toplevel, base_widget, this, m_contextData);
	  addView(ncview);
	}

  htmlview->hide();
  XtVaSetValues(ncview->getBaseWidget(),
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_NONE,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNresizable,  False,
	    XmNwidth, 300,
		NULL);
  XtVaSetValues(htmlview->getBaseWidget(),
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, ncview->getBaseWidget(),
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);
  htmlview->show();
  ncview->show();
  NavCenterShown = True;

}


void
XFE_BrowserView::hideNavCenter()
{
  htmlview->hide();
  ncview->hide();
  XtVaSetValues(htmlview->getBaseWidget(),
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);
  htmlview->show();
  NavCenterShown = False;
}

Boolean
XFE_BrowserView::isNavCenterShown(void) {
  return NavCenterShown;

}

XFE_BrowserView::~XFE_BrowserView()
{
  
  XtDestroyWidget(base_widget);
}

XFE_NavCenterView * XFE_BrowserView::getNavCenterView() {
  return ncview;
}


XFE_HTMLView * XFE_BrowserView::getHTMLView() {
  return htmlview;
}
