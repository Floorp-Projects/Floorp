/* -*- Mode: C++; tab-width: 4 -*-
   BrowserView.h -- class definition for the browser view  class
   Created: Radha Kulkarni <radha@netscape.com>, 23-Feb-1998
 */

/* Insert copyright and license here 1998 */

#ifndef _xfe_browserview_h
#define _xfe_browserview_h

#include "NavCenterView.h"
#include "HTMLView.h"


#include "View.h"

class XFE_BrowserView : public XFE_View
{
public:
  XFE_BrowserView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context);

  virtual ~XFE_BrowserView();

  XFE_HTMLView * getHTMLView();
  XFE_NavCenterView * getNavCenterView();
  Boolean  isNavCenterShown();
  void     hideNavCenter();
  void     showNavCenter();


   /* These are the methods that views will want to overide to add
     their own functionality. */

#ifdef UNDEF
  /* this method is used by the toplevel to sensitize menu/toolbar items. */
  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* m = NULL);

  /* this method is used by the toplevel to dispatch a command. */
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* i = NULL);

  /* used by toplevel to see which view can handle a command.  Returns true
     if we can handle it. */
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL, 
								 XFE_CommandInfo* i = NULL);

  /* used by toplevel to change the labels specified in menu items.  Return NULL
     if no change. */
  virtual char* commandToString(CommandType cmd, void *calldata = NULL,
								XFE_CommandInfo* i = NULL);

  /* used by toplevel to change the selection state of specified toggle menu 
     items.  This method only applies to toggle button */
  virtual Boolean isCommandSelected(CommandType cmd, void *calldata = NULL,
									XFE_CommandInfo* i = NULL);


  virtual XFE_Command* getCommand(CommandType) { return NULL; };
  virtual XFE_View*    getCommandView(XFE_Command*);
#endif  /* UNDEF */
protected:
  XFE_HTMLView       *  htmlview;
  XFE_NavCenterView  *  ncview;
private:
  Widget            base_widget;
  Boolean           NavCenterShown;
  
};



#endif   /* _xfe_browserview_h_  */

