/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
   DesktopType.h -- class definitions for Netscape desktop data types
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 25 Jan 96
 */



#ifndef _DESKTOP_TYPES_H
#define _DESKTOP_TYPES_H

// Classes in this file:
//      XFE_DesktopType
//      XFE_URLDesktopType
//

#include <stdlib.h>
#include <Xm/Xm.h>

//
// Detect Netscape desktop file and extract URL
// nyi - extract other data such as title

extern "C" int XFE_DesktopTypeTranslate(const char*url,char**,int);

//
// XFE_DesktopType
//

class XFE_DesktopType
{
public:
    XFE_DesktopType();
    virtual ~XFE_DesktopType();
    virtual void cleanupDataFiles() { };

    static char *createTmpDirectory();

protected:

    char *_tmpDirectory;
private:
};


//
// XFE_URLDesktopType
// Used in Browser, ProxyIcon, Bookmarks, History

class XFE_URLDesktopType : public XFE_DesktopType
{
public:
    XFE_URLDesktopType(const char *string=NULL);
    XFE_URLDesktopType(FILE*);
    virtual ~XFE_URLDesktopType();
    
    static const char *ftrType();
    static int isDesktopType(const char*);

    // to/from translation
    void setString(const char *);
    const char *getString();

    // list management
    int numItems();
    void createItemList(int);
    void freeItemList();

    // file management
    void writeDataFiles(int);
    void cleanupDataFiles();
    char *getFilename(int pos);
    void readDataFile(FILE*);

    // data access
    const char *url(int);
    const char *title(int);
    const char *description(int);
    const char *filename(int); // provided by writeDataFiles()
    void url(int,const char*);
    void title(int,const char*);
    void description(int,const char*);
protected:
    int _numItems;
    char **_url;
    char **_title;
    char **_description;

    char **_filename;
private:
};

//
// XFE_WebJumperDesktopType
// Read-only compat. with SGI desktop type (valid for all platforms)

class XFE_WebJumperDesktopType : public XFE_DesktopType
{
public:
    XFE_WebJumperDesktopType(FILE*);
    virtual ~XFE_WebJumperDesktopType();

    static int isDesktopType(const char*);

    // file management
    void readDataFile(FILE*);

    // data access
    const char *url();
protected:
    char *_url;
private:
};


#endif // _DESKTOP_TYPES_H
