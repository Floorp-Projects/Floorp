/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   DesktopTypes.cpp -- Desktop type classes for Netscape data on X desktop
   Created: Alastair Gourlay(SGI) c/o Dora Hsu<dora@netscape.com>, 25 Jan 97
 */



// Classes in this file:
//      XFE_DesktopType
//

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <xp.h>
#include <xp_mem.h>
#include <xp_str.h>
#include "DesktopTypes.h"

#ifdef DEBUG_sgidev
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

//
// XFE_DesktopType - base for all desktop types
//

// external access for old xfe code - if a url points to a URL file (NetscapeURL, WebJumper)
// then extract the URL and return it.
//
// Note:'translateWebJumper' determines whether SGI WebJumpers will be resolved or not.
// In some cases, we want to keep them as indirect jumpers (attaching to an email.)
// In other cases we want to resolve them (opening a WebJumper in the browser.)
extern "C" int XFE_DesktopTypeTranslate(const char *url,char **desktop_url,int translateWebJumper) {
    if (desktop_url==NULL)
        return FALSE;

    *desktop_url=NULL;
    
    if (url==NULL || strlen(url)==0)
        return FALSE;    
    
    // if url points to a local file, extract the filename
    char *filename;
    if (strncmp(url,"file:",5)==0 && strlen(url+5)>0)
        filename=XP_STRDUP(url+5);
    else if (strncmp(url,"/",1)==0)
        filename=XP_STRDUP(url);
    else
        return FALSE;

    // strip trailing spaces
    int i=strlen(filename);
    while (isspace(filename[--i]))
        filename[i]='\0';
    
    int retval=FALSE;

    // open file and see if it's a Netscape desktop file
    FILE *fp;
    if (fp=fopen(filename,"r")) {
        const int MAX_LENGTH=4000;
        char line[MAX_LENGTH+1];
        line[MAX_LENGTH]='\0';
        line[0]='\0'; // ensure string will be null-terminated
        fgets(line,MAX_LENGTH,fp);

        // NetscapeURL
        if (XFE_URLDesktopType::isDesktopType(line)) {
            XFE_URLDesktopType urlData(fp);
            if (urlData.numItems()>0) {
                *desktop_url=XP_STRDUP(urlData.url(0));
                retval=TRUE;
            }
        }
        // WebJumper
        else if (translateWebJumper && XFE_WebJumperDesktopType::isDesktopType(line)) {
            XFE_WebJumperDesktopType wjData(fp);
            if (wjData.url()) {
                *desktop_url=XP_STRDUP(wjData.url());
                retval=TRUE;
            }
        }   
        // nyi - check for other Netscape desktop types here
        
        // file is not a Netscape desktop file
        else {
            retval=FALSE;
        }
        
        fclose(fp);
    }
    return retval;
}


// constructor

XFE_DesktopType::XFE_DesktopType()
{
    _tmpDirectory=NULL;
}


// destructor

XFE_DesktopType::~XFE_DesktopType()
{
}

// utilities

// create tmp directory for drag and drop
// Returned string  must be released with free()
char * XFE_DesktopType::createTmpDirectory()
{
    char *tmpDirectory=NULL;
    
    // get tmp directory and file name
    if ((tmpDirectory=tempnam(NULL,"nsdnd"))==NULL) {
        return NULL;
    }

    // make sure it's a good dir name
    if (strlen(tmpDirectory)==0) {
        free((void*)tmpDirectory);
        return NULL;
    }
    
    // create tmp directory, access: -rwxr-xr-x
    if (mkdir(tmpDirectory,S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)!=0) {
        free((void*)tmpDirectory);
        return NULL;
    }

    // directory created successfully
    return tmpDirectory;
}



//
// XFE_URLDesktopType
//

// constructor

XFE_URLDesktopType::XFE_URLDesktopType(const char *string) : XFE_DesktopType()
{
    _numItems=0;
    _url=NULL;
    _title=NULL;
    _description=NULL;
    _filename=NULL;
    
    if (string) {
        setString(string);
    }
}

XFE_URLDesktopType::XFE_URLDesktopType(FILE *fp)
{
    _numItems=0;
    _url=NULL;
    _title=NULL;
    _description=NULL;
    _filename=NULL;

    readDataFile(fp);
}

// destructor

XFE_URLDesktopType::~XFE_URLDesktopType()
{
    freeItemList();
}

// FTR type - for SGI desktop
const char *XFE_URLDesktopType::ftrType()
{
    return "NetscapeURL";
}

// check if file contains NetscapeURL data
// Must pass this method the first line of the file!
int XFE_URLDesktopType::isDesktopType(const char *firstLine)
{
    return (strncmp("#NetscapeURL",firstLine,12)==0)?TRUE:FALSE;
}

//
// list management
//

int XFE_URLDesktopType::numItems()
{
    return _numItems;
}

void XFE_URLDesktopType::createItemList(int listSize)
{
    if (_numItems>0)
        freeItemList();

    _url=new char*[listSize];
    _title=new char*[listSize];
    _description=new char*[listSize];
    _filename=new char*[listSize];
    _numItems=listSize;

    for (int i=0;i<listSize;i++) {
        _url[i]=NULL;
        _title[i]=NULL;
        _description[i]=NULL;
        _filename[i]=NULL;
    }
}

void XFE_URLDesktopType::freeItemList()
{
    for (int i=0;i<_numItems;i++) {
        if (_url[i]) { XP_FREE(_url[i]); _url[i]=NULL; }
        if (_title[i]) { XP_FREE(_title[i]); _title[i]=NULL; }
        if (_description[i]) { XP_FREE(_description[i]); _description[i]=NULL; }
        if (_filename && _filename[i]) { XP_FREE(_filename[i]); _filename[i]=NULL; }
    }
    if (_url) { delete _url; _url=NULL; }
    if (_title) { delete _title; _title=NULL; }
    if (_description) { delete _description; _description=NULL; }
    if (_filename) { delete _filename; _filename=NULL; }

    _numItems=0;
}

//
// data access
//
const char*XFE_URLDesktopType::url(int pos)
{
    if (pos<0 || pos>=_numItems)
        return NULL;

    return _url[pos];
}

void XFE_URLDesktopType::url(int pos,const char *string)
{
    if (pos<0 || pos>=_numItems)
        return;
    
    if (_url[pos]) XP_FREE(_url[pos]);
    if (string)
        _url[pos]=XP_STRDUP(string);
    else
        _url[pos]=NULL;
}

const char*XFE_URLDesktopType::title(int pos)
{
    if (pos<0 || pos>=_numItems)
        return NULL;

    return _title[pos];
}

void XFE_URLDesktopType::title(int pos,const char *string)
{
    if (pos<0 || pos>=_numItems)
        return;
    
    if (_title[pos]) XP_FREE(_title[pos]);
    if (string)
        _title[pos]=XP_STRDUP(string);
    else
        _title[pos]=NULL;
}

const char *XFE_URLDesktopType::description(int pos)
{
    if (pos<0 || pos>=_numItems)
        return NULL;

    return _description[pos];
}

void XFE_URLDesktopType::description(int pos,const char *string)
{
    if (pos<0 || pos>=_numItems)
        return;
    
    if (_description[pos]) XP_FREE(_description[pos]);
    if (string)
        _description[pos]=XP_STRDUP(string);
    else
        _description[pos]=NULL;
}

const char *XFE_URLDesktopType::filename(int pos)
{
    if (pos<0 || pos>=_numItems)
        return NULL;

    return _filename[pos];
}

//
// translation to/from string format
//
void XFE_URLDesktopType::setString(const char *string)
{
    // nyi idea - translate spc to +, separate items with spc
    createItemList(1);
    url(0,string);
}

const char *XFE_URLDesktopType::getString()
{
    if (_numItems==0)
        return NULL;
    
    return url(0);
}


//
// file managment
//

// write temporary files for transferring internal data to desktop
// if application wants files dragged as symbolic links, oblige it, and
// just return a list of file names.
void XFE_URLDesktopType::writeDataFiles(int dragFilesAsLinks)
{
    if (_tmpDirectory!=NULL)
        return;

    if (_numItems==0)
        return;

    if (!dragFilesAsLinks) {
        // if any items need to be written as files, create a tmp directory for them.
        if ((_tmpDirectory=createTmpDirectory())==NULL)
            return;
    }
    
    XDEBUG(printf("XFE_URLDesktopType::writeDataFiles(%s)\n",_tmpDirectory));
    
    
    int i;
    
    // build return list with tmp files or paths as necessary
    for (i=0;i<_numItems; i++) {
        if (_url[i]==NULL || strlen(_url[i])==0)
            _filename[i]=NULL;
        else if (dragFilesAsLinks) {
            XDEBUG(printf("  %s\n",_url[i]));
            
            // Return filename instead of indirecting through a NetscapeURL file
            // cleanupDataFiles() is smart enough not to delete the original file in this case.
            if (strncmp(_url[i],"file:",5)==0 && strlen(_url[i]+5)>0)
                _filename[i]=XP_STRDUP(_url[i]+5);
            else if (strlen(_url[i])>0)
                _filename[i]=XP_STRDUP(_url[i]);
            else
                _filename[i]=NULL;
            
            // strip trailing '/', or IRIX desktop will get confused by directory drop.
            if (_filename[i]) {
                char *lastchar=_filename[i]+strlen(_filename[i])-1;
                if (*lastchar=='/')
                    *lastchar='\0';
            }	    
            XDEBUG(printf("       %s\n",_filename[i]));
        }
        else {
            // create a tmp file containing URL etc. for each item
            char *fname=getFilename(i);
            _filename[i]=(char*)XP_ALLOC(strlen(_tmpDirectory)+1+strlen(fname)+1);
            sprintf(_filename[i],"%s/%s",_tmpDirectory,fname);
            XP_FREE(fname);
            FILE *fp;
            if (fp=fopen(_filename[i],"w")) {
                fprintf(fp,"#NetscapeURL\n\n%s\n",getString());
                fclose(fp);
            }
            else {
                XP_FREE(_filename[i]);
                _filename[i]=NULL;
            }
        }
    }
}

// nyi - extract nice file name from title or url
char *XFE_URLDesktopType::getFilename(int pos)
{
    if (pos<0 || pos>=_numItems || _url[pos]==NULL)
        return XP_STRDUP("unknown");

    // (alastair) code taken from libmsg/msgsend.cpp - be nice just to call into libmsg
    // modified slightly to avoid bug of trailing / generating empty name
    // modified to return truncated label for mail/news URL's

    const char *url=_url[pos];
    
    if (!url || strlen(url)==0)
        return XP_STRDUP("(null)");

    char *s;
    char *s2;
    char *s3;
   
    /* If we know the URL doesn't have a sensible file name in it,
       don't bother emitting a content-disposition. */
    if (!XP_STRNCASECMP(url, "news:", 5))
        return XP_STRDUP("news:");
    if (!XP_STRNCASECMP(url, "snews:", 6))
        return XP_STRDUP("snews:");
    if (!XP_STRNCASECMP(url, "mailbox:", 8))
        return XP_STRDUP("mailbox:");

    char *tmpLabel = XP_STRDUP(url);

    s=tmpLabel;
    /* remove trailing / or \ */
    int len=strlen(s);
    if (s[len-1]=='/' || s[len-1]=='\\')
        s[len-1]='\0';

    s2 = XP_STRCHR (s, ':');
    if (s2) s = s2 + 1;
    /* Take the part of the file name after the last / or \ */
    if (s2 = XP_STRRCHR (s, '/'))
        s = s2+1;
    else if (s2 = XP_STRRCHR (s, '\\'))
        s = s2+1;

    /* if it's a non-file url do some additional massaging */
    if (XP_STRNCASECMP(url,"file:",5)!=0 && url[0]!='/') {
        /* Trim off any named anchors or search data. */
        s3 = XP_STRCHR (s, '?');
        if (s3) *s3 = 0;
        s3 = XP_STRCHR (s, '#');
        if (s3) *s3 = 0;

        /* Check for redundant document name.
         * Use previous URL component to provide more meaningful file name.
         */
        if (s2 &&
            XP_STRCASECMP(s,"index.html")==0 ||
            XP_STRCASECMP(s,"index.htm")==0 ||
            XP_STRCASECMP(s,"index.cgi")==0 ||
            XP_STRCASECMP(s,"index.shtml")==0 ||
            XP_STRCASECMP(s,"home.html")==0 ||
            XP_STRCASECMP(s,"home.htm")==0 ||
            XP_STRCASECMP(s,"home.cgi")==0 ||
            XP_STRCASECMP(s,"home.shtml")==0) {
            /* Trim redundant component and try again */
            *s2='\0';
            s=tmpLabel;
            /* Redo: Take the part of the file name after the last / or \ */
            if (s2 = XP_STRRCHR (s, '/'))
                s = s2+1;
            else if (s2 = XP_STRRCHR (s, '\\'))
                s = s2+1;
        }
    }

    /* Now lose the %XX crap. */
    NET_UnEscape (s);

    char *retLabel=XP_STRDUP(s);
    XP_FREE(tmpLabel);

    return retLabel;
}


// Free filename list
// If tmp files were written, remove tmp files and directory

void XFE_URLDesktopType::cleanupDataFiles()
{
    XDEBUG(printf("XFE_URLDesktopType::cleanupDataFiles(%s)\n",_tmpDirectory));
    
    int i;
    for (i=0;i<_numItems; i++) {
        if (_filename[i]) {
            XDEBUG(printf(" %s:\n",_filename[i])); 
            if (_tmpDirectory && strncmp(_filename[i],_tmpDirectory,strlen(_tmpDirectory))==0) {
                XDEBUG(printf("    DELETED\n"));
                unlink(_filename[i]);
            }
            XP_FREE(_filename[i]);
            _filename[i]=NULL;
        }        
    }

    if (_tmpDirectory) {
        rmdir(_tmpDirectory);
        free((void*)_tmpDirectory);
        _tmpDirectory=NULL;
    }
}

//
// construct list from data file
//

void XFE_URLDesktopType::readDataFile(FILE *fp)
{
    const int MAX_LENGTH=4000;
    
    // nyi - concat into a big string, let setString decode it...
    char line[MAX_LENGTH+1];
    line[MAX_LENGTH]='\0';
    while (fgets(line,MAX_LENGTH,fp)) {
        // nuke the trailing \n
        char *nl=strchr(line,'\n');
        if (nl) *nl='\0';
        if (NET_URL_Type(line)) {
            setString(line);
            return;
        }
    }
}



//
// XFE_WebJumperDesktopType
// read-only, for SGI desktop compatibility. Valid for all platforms.
//

// constructor

XFE_WebJumperDesktopType::XFE_WebJumperDesktopType(FILE *fp)
{
    _url=NULL;

    readDataFile(fp);
}

// destructor

XFE_WebJumperDesktopType::~XFE_WebJumperDesktopType()
{
    if (_url)
        XP_FREE(_url);    
}

// check if file contains WebJumper data
// Must pass this method the first line of the file!
int XFE_WebJumperDesktopType::isDesktopType(const char *firstLine)
{
    return (strncmp("#SGIWebJumpsite",firstLine,15)==0) ? TRUE:FALSE;
}

//
// data access
//
const char *XFE_WebJumperDesktopType::url()
{
    return _url;
}


//
// extract data from file
//

void XFE_WebJumperDesktopType::readDataFile(FILE *fp)
{
    const int MAX_LENGTH=4000;
    
    char line[MAX_LENGTH+1];
    line[MAX_LENGTH]='\0';
    while (fgets(line,MAX_LENGTH,fp)) {
        // nuke the trailing \n
        char *nl=strchr(line,'\n');
        if (nl) *nl='\0';
        if (NET_URL_Type(line)) {
            // it's a url
            if (_url)
                XP_FREE(_url);
            _url=XP_STRDUP(line);
            return;
        }
    }
}

