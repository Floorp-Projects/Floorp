/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "prtypes.h"
#include "prstrm.h"
#include <string.h>

const PRIntn STRM_BUFSIZ = 8192;

PRfilebuf::PRfilebuf():
_fd(0),
_opened(PR_FALSE),
_allocated(PR_FALSE)
{
}

PRfilebuf::PRfilebuf(PRFileDesc *filedesc):
streambuf(),
_fd(filedesc),
_opened(PR_FALSE),
_allocated(PR_FALSE)
{
}

PRfilebuf::PRfilebuf(PRFileDesc *filedesc, char * buffptr, int bufflen):
_fd(filedesc),
_opened(PR_FALSE),
_allocated(PR_FALSE)
{
    PRfilebuf::setbuf(buffptr, bufflen);
}

PRfilebuf::~PRfilebuf()
{
    if (_opened){
        close();
    }else
        sync();
	if (_allocated)
		delete base();
}

PRfilebuf*	
PRfilebuf::open(const char *name, int mode, int flags)
{
     if (_fd != 0)
        return 0;    // error if already open
     PRIntn PRmode = 0;
    // translate mode argument
    if (!(mode & ios::nocreate))
        PRmode |= PR_CREATE_FILE;
    //if (mode & ios::noreplace)
    //    PRmode |= O_EXCL;
    if (mode & ios::app){
        mode |= ios::out;
        PRmode |= PR_APPEND;
    }
    if (mode & ios::trunc){
        mode |= ios::out;  // IMPLIED
        PRmode |= PR_TRUNCATE;
    }
    if (mode & ios::out){
        if (mode & ios::in)
            PRmode |= PR_RDWR;
        else
            PRmode |= PR_WRONLY;
        if (!(mode & (ios::in|ios::app|ios::ate|ios::noreplace))){
            mode |= ios::trunc; // IMPLIED
            PRmode |= PR_TRUNCATE;
        }
    }else if (mode & ios::in)
        PRmode |= PR_RDONLY;
    else
        return 0;    // error if not ios:in or ios::out


    //
    // The usual portable across unix crap...
    // NT gets a hokey piece of junk layer that prevents
    // access to the API.
#ifdef WIN32
    _fd = PR_Open(name, PRmode, PRmode);
#else
    _fd = PR_Open(name, PRmode, flags);
#endif
    if (_fd == 0)
        return 0;
    _opened = PR_TRUE;
    if ((!unbuffered()) && (!ebuf())){
        char * sbuf = new char[STRM_BUFSIZ];
        if (!sbuf)
            unbuffered(1);
        else{
			_allocated = PR_TRUE;
            streambuf::setb(sbuf,sbuf+STRM_BUFSIZ,0);
		}
    }
    if (mode & ios::ate){
        if (seekoff(0,ios::end,mode)==EOF){
            close();
            return 0;
        }
    }
    return this;
}

PRfilebuf*	
PRfilebuf::attach(PRFileDesc *filedesc)
{
    _opened = PR_FALSE;
    _fd = filedesc;
    return this;
}

int	
PRfilebuf::overflow(int c)
{
    if (allocate()==EOF)        // make sure there is a reserve area
        return EOF;
    if (PRfilebuf::sync()==EOF) // sync before new buffer created below
        return EOF;

    if (!unbuffered())
        setp(base(),ebuf());

    if (c!=EOF){
        if ((!unbuffered()) && (pptr() < epptr())) // guard against recursion
            sputc(c);
        else{
            if (PR_Write(_fd, &c, 1)!=1)
                return(EOF);
        }
    }
    return(1);  // return something other than EOF if successful
}

int	
PRfilebuf::underflow()
{
    int count;
    unsigned char tbuf;

    if (in_avail())
        return (int)(unsigned char) *gptr();

    if (allocate()==EOF)        // make sure there is a reserve area
        return EOF;
    if (PRfilebuf::sync()==EOF)
        return EOF;

    if (unbuffered())
        {
        if (PR_Read(_fd,(void *)&tbuf,1)<=0)
            return EOF;
        return (int)tbuf;
        }

    if ((count=PR_Read(_fd,(void *)base(),blen())) <= 0)
        return EOF;     // reached EOF
    setg(base(),base(),base()+count);
    return (int)(unsigned char) *gptr();
}

streambuf*	
PRfilebuf::setbuf(char *buffptr, int bufflen)
{
    if (is_open() && (ebuf()))
        return 0;
    if ((!buffptr) || (bufflen <= 0))
        unbuffered(1);
    else
        setb(buffptr, buffptr+bufflen, 0);
    return this;
}

streampos	
PRfilebuf::seekoff(streamoff offset, ios::seek_dir dir, int /* mode */)
{
    if (PR_GetDescType(_fd) == PR_DESC_FILE){
        PRSeekWhence fdir;
        PRInt32 retpos;
        switch (dir) {
            case ios::beg :
                fdir = PR_SEEK_SET;
                break;
            case ios::cur :
                fdir = PR_SEEK_CUR;
                break;
            case ios::end :
                fdir = PR_SEEK_END;
                break;
            default:
            // error
                return(EOF);
            }

        if (PRfilebuf::sync()==EOF)
            return EOF;
        if ((retpos=PR_Seek(_fd, offset, fdir))==-1L)
            return (EOF);
        return((streampos)retpos);
    }else
        return (EOF);
}


int 
PRfilebuf::sync()
{
    PRInt32 count; 

    if (_fd==0)
        return(EOF);

    if (!unbuffered()){
        // Sync write area
        if ((count=out_waiting())!=0){
            PRInt32 nout;
            if ((nout =PR_Write(_fd,
                               (void *) pbase(),
                               (unsigned int)count)) != count){
                if (nout > 0) {
                    // should set _pptr -= nout
                    pbump(-(int)nout);
                    memmove(pbase(), pbase()+nout, (int)(count-nout));
                }
                return(EOF);
            }
        }
        setp(0,0); // empty put area

        if (PR_GetDescType(_fd) == PR_DESC_FILE){
            // Sockets can't seek; don't need this
            if ((count=in_avail()) > 0){
                if (PR_Seek(_fd, -count, PR_SEEK_CUR)!=-1L)
                {
                    return (EOF);
                }
            }
        }
        setg(0,0,0); // empty get area
    }
    return(0);
}

PRfilebuf * 
PRfilebuf::close()
{
    int retval;
    if (_fd==0)
        return 0;

    retval = sync();

    if ((PR_Close(_fd)==0) || (retval==EOF))
        return 0;
    _fd = 0;
    return this;
}

PRofstream::PRofstream():
ostream(new PRfilebuf)
{
    _PRSTR_DELBUF(0);
}

PRofstream::PRofstream(PRFileDesc *filedesc):
ostream(new PRfilebuf(filedesc))
{
    _PRSTR_DELBUF(0);
}

PRofstream::PRofstream(PRFileDesc *filedesc, char *buff, int bufflen):
ostream(new PRfilebuf(filedesc, buff, bufflen))
{
    _PRSTR_DELBUF(0);
}

PRofstream::PRofstream(const char *name, int mode, int aFlags):
ostream(new PRfilebuf)
{
    _PRSTR_DELBUF(0);
    if (!rdbuf()->open(name, (mode|ios::out), aFlags))
        clear(rdstate() | ios::failbit);
}

PRofstream::~PRofstream()
{
	flush();

	delete rdbuf();
#ifdef _PRSTR_BP
	_PRSTR_BP = 0;
#endif
}

streambuf * 
PRofstream::setbuf(char * ptr, int len)
{
    if ((is_open()) || (!(rdbuf()->setbuf(ptr, len)))){
        clear(rdstate() | ios::failbit);
        return 0;
    }
    return rdbuf();
}

void 
PRofstream::attach(PRFileDesc *filedesc)
{
    if (!(rdbuf()->attach(filedesc)))
        clear(rdstate() | ios::failbit);
}

void 
PRofstream::open(const char * name, int mode, int aFlags)
{
    if (is_open() || !(rdbuf()->open(name, (mode|ios::out), aFlags)))
        clear(rdstate() | ios::failbit);
}

void 
PRofstream::close()
{
    clear((rdbuf()->close()) ? 0 : (rdstate() | ios::failbit));
}



