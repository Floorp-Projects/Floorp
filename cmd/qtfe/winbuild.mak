

ICONS	=	icons/images/BM_Bookmark.gif \
		icons/images/BM_Change.gif \
		icons/images/BM_Folder.gif \
		icons/images/BM_FolderO.gif \
		icons/images/BM_MailBookmark.gif \
		icons/images/BM_MenuFolder.gif \
		icons/images/BM_MenuFolderO.gif \
		icons/images/BM_NewAndMenuFolder.gif \
		icons/images/BM_NewAndMenuFolderO.gif \
		icons/images/BM_NewFolder.gif \
		icons/images/BM_NewFolderO.gif \
		icons/images/BM_NewPersonalFolder.gif \
		icons/images/BM_NewPersonalFolderO.gif \
		icons/images/BM_NewPersonalMenu.gif \
		icons/images/BM_NewPersonalMenuO.gif \
		icons/images/BM_NewsBookmark.gif \
		icons/images/BM_PersonalFolder.gif \
		icons/images/BM_PersonalFolderO.gif \
		icons/images/BM_PersonalMenu.gif \
		icons/images/BM_PersonalMenuO.gif \
		icons/images/BM_QFile.gif \
		icons/images/BM_QFile.i.gif \
		icons/images/BM_QFile.md.gif \
		icons/images/BM_QFile.mo.gif \
		icons/images/TB_Back.gif \
		icons/images/TB_Back.i.gif \
		icons/images/TB_Back.md.gif \
		icons/images/TB_Back.mo.gif \
		icons/images/TB_Edit.gif \
		icons/images/TB_Edit.i.gif \
		icons/images/TB_Edit.md.gif \
		icons/images/TB_Edit.mo.gif \
		icons/images/TB_EditPage.gif \
		icons/images/TB_EditPage.i.gif \
		icons/images/TB_EditPage.md.gif \
		icons/images/TB_EditPage.mo.gif \
		icons/images/TB_Forward.gif \
		icons/images/TB_Forward.i.gif \
		icons/images/TB_Forward.md.gif \
		icons/images/TB_Forward.mo.gif \
		icons/images/TB_Home.gif \
		icons/images/TB_Home.i.gif \
		icons/images/TB_Home.md.gif \
		icons/images/TB_Home.mo.gif \
		icons/images/TB_LoadImages.gif \
		icons/images/TB_LoadImages.i.gif \
		icons/images/TB_LoadImages.md.gif \
		icons/images/TB_LoadImages.mo.gif \
		icons/images/TB_MixSecurity.gif \
		icons/images/TB_MixSecurity.i.gif \
		icons/images/TB_MixSecurity.md.gif \
		icons/images/TB_MixSecurity.mo.gif \
		icons/images/TB_Places.gif \
		icons/images/TB_Places.i.gif \
		icons/images/TB_Places.md.gif \
		icons/images/TB_Places.mo.gif \
		icons/images/TB_Print.gif \
		icons/images/TB_Print.i.gif \
		icons/images/TB_Print.md.gif \
		icons/images/TB_Print.mo.gif \
		icons/images/TB_Reload.gif \
		icons/images/TB_Reload.i.gif \
		icons/images/TB_Reload.md.gif \
		icons/images/TB_Reload.mo.gif \
		icons/images/TB_Search.gif \
		icons/images/TB_Search.i.gif \
		icons/images/TB_Search.md.gif \
		icons/images/TB_Search.mo.gif \
		icons/images/TB_Secure.gif \
		icons/images/TB_Secure.i.gif \
		icons/images/TB_Secure.md.gif \
		icons/images/TB_Secure.mo.gif \
		icons/images/TB_Stop.gif \
		icons/images/TB_Stop.i.gif \
		icons/images/TB_Stop.md.gif \
		icons/images/TB_Stop.mo.gif \
		icons/images/TB_Unsecure.gif \
		icons/images/TB_Unsecure.i.gif \
		icons/images/TB_Unsecure.md.gif \
		icons/images/TB_Unsecure.mo.gif \
		icons/images/Dash_Secure.gif \
		icons/images/Dash_Unsecure.gif \
		images/biganim.gif \
		images/notanim.gif \
		images/smallanim.gif

SRCMOC	=	moc_callback.cpp \
		moc_contextmenu.cpp \
		moc_FindDialog.cpp \
		moc_mainwindow.cpp \
		moc_OpenDialog.cpp \
		moc_QtBookmarkButton.cpp \
		moc_QtBookmarkEditDialog.cpp \
		moc_QtBookmarkMenu.cpp \
		moc_QtBookmarksContext.cpp \
		moc_QtBrowserContext.cpp \
		moc_QtContext.cpp \
		moc_QtEventPusher.cpp \
		moc_QtPrefs.cpp \
		moc_SaveAsDialog.cpp \
		moc_SecurityDialog.cpp \
		moc_toolbars.cpp

MOC	=	moc

all: icon-pixmaps.inc $(SRCMOC)

icon-pixmaps.inc: $(ICONS) $(SRCMOC)
	mkfiles32\embed $(ICONS) > icon-pixmaps.inc

moc_callback.cpp: callback.h
	$(MOC) callback.h -o moc_callback.cpp

moc_contextmenu.cpp: contextmenu.h
	$(MOC) contextmenu.h -o moc_contextmenu.cpp

moc_FindDialog.cpp: FindDialog.h
	$(MOC) FindDialog.h -o moc_FindDialog.cpp

moc_mainwindow.cpp: mainwindow.h
	$(MOC) mainwindow.h -o moc_mainwindow.cpp

moc_OpenDialog.cpp: OpenDialog.h
	$(MOC) OpenDialog.h -o moc_OpenDialog.cpp

moc_QtBookmarkButton.cpp: QtBookmarkButton.h
	$(MOC) QtBookmarkButton.h -o moc_QtBookmarkButton.cpp

moc_QtBookmarkEditDialog.cpp: QtBookmarkEditDialog.h
	$(MOC) QtBookmarkEditDialog.h -o moc_QtBookmarkEditDialog.cpp

moc_QtBookmarkMenu.cpp: QtBookmarkMenu.h
	$(MOC) QtBookmarkMenu.h -o moc_QtBookmarkMenu.cpp

moc_QtBookmarksContext.cpp: QtBookmarksContext.h
	$(MOC) QtBookmarksContext.h -o moc_QtBookmarksContext.cpp

moc_QtBrowserContext.cpp: QtBrowserContext.h
	$(MOC) QtBrowserContext.h -o moc_QtBrowserContext.cpp

moc_QtContext.cpp: QtContext.h
	$(MOC) QtContext.h -o moc_QtContext.cpp

moc_QtEventPusher.cpp: QtEventPusher.h
	$(MOC) QtEventPusher.h -o moc_QtEventPusher.cpp

moc_QtPrefs.cpp: QtPrefs.h
	$(MOC) QtPrefs.h -o moc_QtPrefs.cpp

moc_SaveAsDialog.cpp: SaveAsDialog.h
	$(MOC) SaveAsDialog.h -o moc_SaveAsDialog.cpp

moc_SecurityDialog.cpp: SecurityDialog.h
	$(MOC) SecurityDialog.h -o moc_SecurityDialog.cpp

moc_toolbars.cpp: toolbars.h
	$(MOC) toolbars.h -o moc_toolbars.cpp

