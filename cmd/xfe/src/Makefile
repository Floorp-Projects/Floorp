#! gmake
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

DEPTH		= ../../..

include $(DEPTH)/config/config.mk

MODULE		= xfe2

USE_3PANE 	= 1

LIBRARY_NAME	= xfe2

REQUIRES	=\
			XfeWidgets \
			XfeBm \
			Microline \
			nspr \
			layer \
			js \
			jtools \
			applet \
			security \
			img \
			util \
			parse \
			layout \
			hook \
			pref \
			java \
			libfont \
			ldap \
			lay \
			style \
			xfeicons \
			rdf \
			xpcom \
			plug \
			netcnvts

CPPSRCS		= \
		  plugin.cpp \
		  BackForwardMenu.cpp \
		  BookmarkBase.cpp \
		  BookmarkFindDialog.cpp \
		  BookmarkMenu.cpp \
		  BookmarkPropDialog.cpp \
		  BookmarkWhatsNewDialog.cpp \
		  Button.cpp \
		  BookmarkFrame.cpp \
		  BookmarkView.cpp \
		  BrowserDrop.cpp \
		  BrowserFrame.cpp \
		  BrowserView.cpp \
		  ColorDialog.cpp \
		  Command.cpp \
		  Component.cpp \
		  DesktopTypes.cpp \
		  Dialog.cpp \
		  DisplayFactory.cpp \
		  Dashboard.cpp \
		  DownloadFrame.cpp \
		  DragDrop.cpp \
		  EditHdrDialog.cpp \
		  FrameListMenu.cpp \
		  Frame.cpp \
		  HTMLDialogFrame.cpp \
		  HistoryMenu.cpp \
		  HistoryView.cpp \
		  HTMLDrag.cpp \
		  HTMLView.cpp \
		  HistoryFrame.cpp \
		  IconGroup.cpp \
		  LocationDrag.cpp \
		  Logo.cpp \
		  Menu.cpp \
		  MenuBar.cpp \
		  Minibuffer.cpp \
		  MozillaApp.cpp \
		  NavCenterFrame.cpp \
		  NavCenterView.cpp \
		  Netcaster.cpp \
		  NotificationCenter.cpp \
		  Outliner.cpp \
		  PersonalToolbar.cpp \
		  PopupMenu.cpp \
		  PrefsDialogAppl.cpp \
		  PropertyTabView.cpp \
		  ProxyIcon.cpp	\
		  PrefsApplEdit.cpp \
		  PrefsDialog.cpp \
		  PrefsDialogBrowser.cpp \
		  PrefsDialogGeneral.cpp \
		  PrefsDialogMailnews.cpp \
		  PrefsLang.cpp \
		  PrefsProxiesView.cpp \
		  RDFView.cpp \
		  RadioGroup.cpp \
		  Splash.cpp \
		  Toolbar.cpp \
		  ToolbarDrop.cpp \
		  Toolbox.cpp \
		  ToolboxItem.cpp \
		  URLBar.cpp \
		  ViewDialog.cpp \
		  ViewGlue.cpp \
		  View.cpp \
		  context_funcs.cpp \
		  pref.cpp \
		  $(NULL)


ifdef USE_MOTIF_DND
CPPSRCS		+= \
		OutlinerDrop.cpp \
		$(NULL)
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  ABDirGenTabView.cpp \
		  ABDirPropertyDlg.cpp \
		  ABSearchDlg.cpp \
		  ABSearchView.cpp \
		  AB2PaneView.cpp \
		  ABDirListView.cpp \
		  ABAddrSearchView.cpp \
		  ABAddresseeView.cpp \
		  ABAddressingDlg.cpp \
		  ABListSearchView.cpp\
		  ABMListDlg.cpp \
		  ABMListView.cpp \
		  ABNameCTalkTab.cpp \
		  ABNameConTab.cpp \
		  ABNameFolderDlg.cpp \
		  ABNameGenTab.cpp \
		  AddrBookFrame.cpp \
		  AddrBookView.cpp \
		  AddressFolderView.cpp \
		  AddressOutliner.cpp \
		  AdvSearchDialog.cpp \
		  AttachPanel.cpp \
		  AttachmentMenu.cpp \
		  ComposeAttachDialog.cpp \
		  ComposeAttachFolderView.cpp \
		  ComposeFolderView.cpp \
		  ComposeFrame.cpp \
		  ComposeView.cpp \
		  $(NULL)
endif

ifdef EDITOR
CPPSRCS		+= \
		  EditorDrop.cpp \
		  EditorFrame.cpp \
		  EditorToolbar.cpp \
		  EditorView.cpp \
		  EditRecentMenu.cpp \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  FolderDropdown.cpp \
		  FolderFrame.cpp \
		  FolderPromptDialog.cpp \
		  FolderMenu.cpp \
		  FolderView.cpp \
		  Frame.cpp \
		  $(NULL)
endif

ifdef MOZ_LDAP
CPPSRCS		+= \
		  LdapSearchFrame.cpp \
		  LdapSearchView.cpp \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  MNBanner.cpp \
		  MNListView.cpp \
		  MNSearchFrame.cpp \
		  MNSearchView.cpp \
		  MNView.cpp \
		  MailDownloadFrame.cpp \
		  MailFilterDlg.cpp \
		  MailFilterRulesDlg.cpp \
		  MailFilterRulesView.cpp \
		  MailFilterView.cpp \
		  MailFolderPropDialog.cpp \
		  MsgFrame.cpp \
		  MsgView.cpp \
		  NewsPromptDialog.cpp \
		  NewsServerDialog.cpp \
		  NewsServerPropDialog.cpp \
		  NewsgroupPropDialog.cpp \
		  OptionFolderView.cpp \
		  $(NULL)
endif

ifdef EDITOR
CPPSRCS		+= \
		  PrefsDialogEditor.cpp \
		  $(NULL)
endif

ifdef MOZ_LDAP
CPPSRCS		+= \
		  PrefsLdapProp.cpp \
		  PrefsMserverMore.cpp \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  PrefsMsgMore.cpp \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  PropertySheetDialog.cpp \
		  PropertySheetView.cpp \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  ReadAttachDrag.cpp \
		  ReadAttachPanel.cpp \
		  SearchRuleView.cpp \
		  SubAllView.cpp \
		  SubNewView.cpp \
		  SubSearchView.cpp \
		  SubTabView.cpp \
		  SubscribeDialog.cpp \
		  SubscribeView.cpp \
		  $(NULL)
endif

ifdef MOZ_TASKBAR
CPPSRCS		+= \
		  TaskBar.cpp \
		  TaskBarDrop.cpp \
		  $(NULL)

DEFINES += -DMOZ_TASKBAR
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  TextEditorView.cpp \
		  $(NULL)
endif

ifdef MOZ_MAIL_NEWS
CPPSRCS		+= \
		  ThreadFrame.cpp \
		  ThreadView.cpp \
		  $(NULL)

ifdef USE_3PANE
CPPSRCS		+= \
		ThreePaneView.cpp \
		$(NULL)
endif
endif

ifdef EDITOR
CPPSRCS		+= \
		  SpellHandler.cpp \
		$(NULL)
endif

CSRCS		= commands.c

include $(DEPTH)/config/rules.mk

INCLUDES	+= -I$(OBJDIR) -I.. -I../DtWidgets -I$(DEPTH)/modules/libreg/include

#ifndef MOZ_LITE
#CFLAGS		+= -DEDITOR
#endif

DEFINES		+= -DNEW_DECODERS -DDELAYED_MENU_CREATION -DNETCASTER_ZAXIS_HACKERY
DEFINES		+= -DUSE_MAGIC_TOOLBAR

ifdef USE_3PANE
DEFINES		+= -DUSE_3PANE
endif

ifdef USE_ABCOM
DEFINES		+= -DUSE_ABCOM
endif

ifdef USE_MOTIF_DND
DEFINES		+= -DUSE_MOTIF_DND
endif


$(LIBRARY): $(OBJS)

# note the header file is generated as a side effect here.
$(OBJDIR)/commands.c $(OBJDIR)/commands.h: $(OBJDIR)/mkcommands xfe_commands
	@echo Generating command table information... ; \
	$(OBJDIR)/mkcommands -table $(OBJDIR)/commands.c -header $(OBJDIR)/commands.h xfe_commands

$(OBJDIR)/commands.o: $(OBJDIR)/commands.c
	$(CC) -c -I. -o $@ $(OBJDIR)/commands.c

$(OBJDIR)/mkcommands: mkcommands.c
	@$(MAKE_OBJDIR)
	$(CCF) -o $@ $<


# This rule is needed for 'make -j' to work since just about everything
# is dependant on commands.h.
$(OBJS): $(OBJDIR)/commands.h

NetcasterIncluders:=Frame.o PrefsDialogGeneral.o Netcaster.o
$(foreach f,$(NetcasterIncluders),$(OBJDIR)/$(f)): Netcaster.h
