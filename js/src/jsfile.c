#include "jsapi.h"
#include "jsstr.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jslock.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsdate.h"

#include "prio.h"
#include "prerror.h"


#include "prlog.h"

/* I don't remember where I ripped these defines from. probably in some XP code. */

#ifdef XP_MAC
#  define LINEBREAK             "\012"
#  define LINEBREAK_LEN 1
#else
#  if defined(_WINDOWS) || defined(OS2)
#    define LINEBREAK           "\015\012"
#    define LINEBREAK_LEN       2
#  else
#    ifdef XP_UNIX
#      define LINEBREAK         "\012"
#      define LINEBREAK_LEN     1
#    endif /* UNIX */
#  endif /* WIN */
#endif /* MAC */


/*-------------------------------------*/
/* The following is ripped from libi18n/unicvt.c and includes.. */

/*
 * UTF8 defines and macros
 */
#define ONE_OCTET_BASE			0x00	/* 0xxxxxxx */
#define ONE_OCTET_MASK			0x7F	/* x1111111 */
#define CONTINUING_OCTET_BASE	0x80	/* 10xxxxxx */
#define CONTINUING_OCTET_MASK	0x3F	/* 00111111 */
#define TWO_OCTET_BASE			0xC0	/* 110xxxxx */
#define TWO_OCTET_MASK			0x1F	/* 00011111 */
#define THREE_OCTET_BASE		0xE0	/* 1110xxxx */
#define THREE_OCTET_MASK		0x0F	/* 00001111 */
#define FOUR_OCTET_BASE			0xF0	/* 11110xxx */
#define FOUR_OCTET_MASK			0x07	/* 00000111 */
#define FIVE_OCTET_BASE			0xF8	/* 111110xx */
#define FIVE_OCTET_MASK			0x03	/* 00000011 */
#define SIX_OCTET_BASE			0xFC	/* 1111110x */
#define SIX_OCTET_MASK			0x01	/* 00000001 */

#define IS_UTF8_1ST_OF_1(x)	(( (x)&~ONE_OCTET_MASK  ) == ONE_OCTET_BASE)
#define IS_UTF8_1ST_OF_2(x)	(( (x)&~TWO_OCTET_MASK  ) == TWO_OCTET_BASE)
#define IS_UTF8_1ST_OF_3(x)	(( (x)&~THREE_OCTET_MASK) == THREE_OCTET_BASE)
#define IS_UTF8_1ST_OF_4(x)	(( (x)&~FOUR_OCTET_MASK ) == FOUR_OCTET_BASE)
#define IS_UTF8_1ST_OF_5(x)	(( (x)&~FIVE_OCTET_MASK ) == FIVE_OCTET_BASE)
#define IS_UTF8_1ST_OF_6(x)	(( (x)&~SIX_OCTET_MASK  ) == SIX_OCTET_BASE)
#define IS_UTF8_2ND_THRU_6TH(x) \
					(( (x)&~CONTINUING_OCTET_MASK  ) == CONTINUING_OCTET_BASE)
#define IS_UTF8_1ST_OF_UCS2(x) \
			IS_UTF8_1ST_OF_1(x) \
			|| IS_UTF8_1ST_OF_2(x) \
			|| IS_UTF8_1ST_OF_3(x)


#define MAX_UCS2			0xFFFF	 
#define DEFAULT_CHAR		0x003F	/* Default char is "?" */ 
#define BYTE_MASK			0xBF
#define BYTE_MARK			0x80


/* Function: one_ucs2_to_utf8_char
 * 
 * Function takes one UCS-2 char and writes it to a UTF-8 buffer.
 * We need a UTF-8 buffer because we don't know before this 
 * function how many bytes of utf-8 data will be written. It also
 * takes a pointer to the end of the UTF-8 buffer so that we don't
 * overwrite data. This function returns the number of UTF-8 bytes
 * of data written, or -1 if the buffer would have been overrun.
 */


#define LINE_SEPARATOR		0x2028
#define PARAGRAPH_SEPARATOR	0x2029
int16 one_ucs2_to_utf8_char(unsigned char *tobufp, 
		unsigned char *tobufendp, uint16 onechar)

{

	 int16 numUTF8bytes = 0;

	if((onechar == LINE_SEPARATOR)||(onechar == PARAGRAPH_SEPARATOR))
	{
		strcpy((char*)tobufp, "\n");
		return strlen((char*)tobufp);;
	}

	 	if (onechar < 0x80) {				numUTF8bytes = 1;
		} else if (onechar < 0x800) {		numUTF8bytes = 2;
		} else if (onechar <= MAX_UCS2) {	numUTF8bytes = 3;
		} else { numUTF8bytes = 2;
				 onechar = DEFAULT_CHAR;
		} 
                
		tobufp += numUTF8bytes;

		/* return error if we don't have space for the whole character */
		if (tobufp > tobufendp) {
			return(-1); 
		}


		switch(numUTF8bytes) {

			case 3: *--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
					*--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
					*--tobufp = onechar |  THREE_OCTET_BASE;
					break;

			case 2: *--tobufp = (onechar | BYTE_MARK) & BYTE_MASK; onechar >>=6;
					*--tobufp = onechar | TWO_OCTET_BASE;
					break;
			case 1: *--tobufp = (unsigned char)onechar;  break;
		}

		return(numUTF8bytes);
}


/*
 * utf8_to_ucs2_char
 *
 * Convert a utf8 multibyte character to ucs2
 *
 * inputs: pointer to utf8 character(s)
 *         length of utf8 buffer ("read" length limit)
 *         pointer to return ucs2 character
 *
 * outputs: number of bytes in the utf8 character
 *          -1 if not a valid utf8 character sequence
 *          -2 if the buffer is too short
 */
int16
utf8_to_ucs2_char(const unsigned char *utf8p, int16 buflen, uint16 *ucs2p)
{
	uint16 lead, cont1, cont2;

	/*
	 * Check for minimum buffer length
	 */
	if ((buflen < 1) || (utf8p == NULL)) {
		return -2;
	}
	lead = (uint16) (*utf8p);

	/*
	 * Check for a one octet sequence
	 */
	if (IS_UTF8_1ST_OF_1(lead)) {
		*ucs2p = lead & ONE_OCTET_MASK;
		return 1;
	}

	/*
	 * Check for a two octet sequence
	 */
	if (IS_UTF8_1ST_OF_2(*utf8p)) {
		if (buflen < 2)
			return -2;
		cont1 = (uint16) *(utf8p+1);
		if (!IS_UTF8_2ND_THRU_6TH(cont1))
			return -1;
		*ucs2p =  (lead & TWO_OCTET_MASK) << 6;
		*ucs2p |= cont1 & CONTINUING_OCTET_MASK;
		return 2;
	}

	/*
	 * Check for a three octet sequence
	 */
	else if (IS_UTF8_1ST_OF_3(lead)) {
		if (buflen < 3)
			return -2;
		cont1 = (uint16) *(utf8p+1);
		cont2 = (uint16) *(utf8p+2);
		if (   (!IS_UTF8_2ND_THRU_6TH(cont1))
			|| (!IS_UTF8_2ND_THRU_6TH(cont2)))
			return -1;
		*ucs2p =  (lead & THREE_OCTET_MASK) << 12;
		*ucs2p |= (cont1 & CONTINUING_OCTET_MASK) << 6;
		*ucs2p |= cont2 & CONTINUING_OCTET_MASK;
		return 3;
	}
	else { /* not a valid utf8/ucs2 character */
		return -1;
	}
}

/*-------------------------------------*/


#define ASCII	0
#define UTF		1
#define UNICODE 2

/* a few forward declarations.. */
static JSClass file_class;
static JSObject*NewFileObject(JSContext *cx, char*bytes);
static JSObject*NewFileObjectCopyZ(JSContext *cx, char*bytes);
static JSBool file_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);


typedef struct JSFile {
	char		*path;	/* the path to the file. */
    PRFileDesc*	handle; /* the handle for the file, if opened */
	JSBool		opened;
	JSString	*linebuffer; /* temp buffer used by readln. */
	int32		mode;   /* mode used to open the file: read, write, append, create, etc.. */
	int32		type;	/* Asciiz, utf, unicode */
	char		byteBuffer[3]; /* bytes read in advance by JS_FileRead ( UTF8 encoding ) */
	char		nbBytesInBuf; /* number of bytes stored in the buffer above */
	jschar		charBuffer; /* character read in advance by readln ( mac files only ) */
	JSBool		charBufferUsed; /* flag indicating if the buffer above is being used */
	JSBool		randomAccess; /* can the file be randomly accessed? false for stdin, and 
								 UTF-encoded files. */
	JSBool		autoflush;	 /* should we force a flush for each line break? */
} JSFile;


void resetBuffers(JSFile * f) {
	f->charBufferUsed=JS_FALSE;
	f->nbBytesInBuf=0;
}


/* Helper function. buffered version of PR_Read. used by JS_FileRead */
int JS_BufferedRead(JSFile * f,char*buf, int len) {
	int count=0;
	while (f->nbBytesInBuf>0&&len>0) {
		buf[0]=f->byteBuffer[0];
		f->byteBuffer[0]=f->byteBuffer[1];
		f->byteBuffer[1]=f->byteBuffer[2];
		f->nbBytesInBuf--;
		len--;
		buf+=1;
		count++;
	}
	if (len>0) {
		count+=PR_Read(f->handle, buf, len);
	}
	return count;
}


int JS_FileRead(JSFile * f,jschar*buf,int len,int32 mode) {
	unsigned char*aux;
	int count,i;
	int remainder;
	unsigned char utfbuf[3];

	if (f->charBufferUsed) {
	  buf[0]=f->charBuffer;
	  buf++;
	  len--;
	  f->charBufferUsed=JS_FALSE;
	}

	switch (mode) {
	case ASCII:
	  aux = malloc(len);
	  if (!aux) {
		return 0;
	  }
	  count=JS_BufferedRead(f,aux,len);
	  if (count==-1) {
		free(aux);
		return 0;
	  }
	  for (i=0;i<len;i++) {
		buf[i]=(jschar)aux[i];
	  }
	  free(aux);
	  break;
	case UTF:
		remainder = 0;
		for (count=0;count<len;count++) {
			i=JS_BufferedRead(f,utfbuf+remainder,3-remainder);
			if (i<=0) {
				return count;
			}
			i=utf8_to_ucs2_char(utfbuf, (int16)i, &buf[count] );
			if (i<0) {
				return count;
			} else {
				if (i==1) {
					utfbuf[0]=utfbuf[1];
					utfbuf[1]=utfbuf[2];
					remainder=2;
				} else
				if (i==2) {
					utfbuf[0]=utfbuf[2];
					remainder=1;
				} else
				if (i==3)
					remainder=0;
			}
		}
		while (remainder>0) {
			f->byteBuffer[f->nbBytesInBuf]=utfbuf[0];
			f->nbBytesInBuf++;
			utfbuf[0]=utfbuf[1];
			utfbuf[1]=utfbuf[2];
			remainder--;
		}
	  break;
	case UNICODE:
	  count=JS_BufferedRead(f,(char*)buf,len*2)>>1;
	  if (count==-1) {
		  return 0;
	  }
	  break;
	}
	return count;
}

int JS_FileSkip(JSFile * f, int len, int32 mode) {
	int count,i;
	int remainder;
	unsigned char utfbuf[3];
	jschar tmp;

	switch (mode) {
	case ASCII:
	  count=PR_Seek(f->handle,len,PR_SEEK_CUR);
	  break;
	case UTF:
		remainder = 0;
		for (count=0;count<len;count++) {
			i=JS_BufferedRead(f,utfbuf+remainder,3-remainder);
			if (i<=0) {
				return 0;
			}
			i=utf8_to_ucs2_char(utfbuf, (int16)i, &tmp );
			if (i<0) {
				return 0;	
			} else {
				
				if (i==1) {
					utfbuf[0]=utfbuf[1];
					utfbuf[1]=utfbuf[2];
					remainder=2;
				} else
				if (i==2) {
					utfbuf[0]=utfbuf[2];
					remainder=1;
				} else
				if (i==3)
					remainder=0;
			}
		}
		while (remainder>0) {
			f->byteBuffer[f->nbBytesInBuf]=utfbuf[0];
			f->nbBytesInBuf++;
			utfbuf[0]=utfbuf[1];
			utfbuf[1]=utfbuf[2];
			remainder--;
		}
	  break;
	case UNICODE:
	  count=PR_Seek(f->handle,len*2,PR_SEEK_CUR)/2;
	  break;
	}
	return count;
}



int JS_FileWrite(JSFile* f, jschar*buf, int len, int32 mode) {
	unsigned char*aux;
	int count,i,j;
	unsigned char*utfbuf;
	switch (mode) {
	case ASCII:
	  aux = malloc(len); // check out of memory..
	  if (!aux) {
		  return 0;
	  }
	  for (i=0; i<len; i++) {
		aux[i]=buf[i]%256;
	  }
	  count = PR_Write(f->handle,aux,len);
	  if (count==-1) {
		  free(aux);
		  return 0;
	  }
	  free(aux);
	  break;
	case UTF: // this time, block conversion should be possible..
		utfbuf = malloc(len*3);
		if (!utfbuf) { //panic..
			return 0;
		}
		i=0;
		for (count=0;count<len;count++) {
			j=one_ucs2_to_utf8_char(utfbuf+i,utfbuf+len*3,buf[count]);
			if (j==-1) { // more panic..
				return 0;
			}
			i+=j;
		}
		j=PR_Write(f->handle,utfbuf,i);
		if (j<i) {
			return 0;
		}
	  break;
	case UNICODE:
	  count = PR_Write(f->handle,buf,len*2)>>1;
	  if (count==-1) {
		  return 0;
	  }
	  break;
	}
	return count;
}




static JSBool
file_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile *file;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,file->path));
	return JS_TRUE;
}

/* Ripped off from lm_win.c .. */
/* where is strcasecmp?.. for now, it's case sensitive.. */
static int32
file_has_option(char *options, char *name)
{
    char *comma, *equal;
    int32 found = 0;

    for (;;) {
        comma = strchr(options, ',');
        if (comma) *comma = '\0';
        equal = strchr(options, '=');
        if (equal) *equal = '\0';
        if (strcmp(options, name) == 0) {
            if (!equal || strcmp(equal + 1, "yes") == 0)
                found = 1;
            else
                found = atoi(equal + 1);
        }
        if (equal) *equal = '=';
        if (comma) *comma = ',';
        if (found || !comma)
            break;
        options = comma + 1;
    }
    return found;
}



static JSBool
file_open(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile	*file;
	JSString *strmode, *strtype;
	char	*ctype;
	char	*mode;
	int32		mask;
	int32		type;
	PRFileDesc *handle;
	char	debug[200];

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (file->opened) {
		file_close(cx,obj,0,NULL,NULL);
	}

	if (!file->path)
	return JS_FALSE;

	if (argc>=2) {
	  strmode =JS_ValueToString(cx, argv[1]);
	  if (!strmode ) {
	    JS_ReportError(cx,"The first argument to file.open must be a string.");
	    return JS_FALSE;
	  }
	  mode = JS_GetStringBytes(strmode);
	} else
	  mode = "readWrite,append,create"; /* non-destructive, permissive defaults. */
	if (argc>=1) {
	  strtype = JS_ValueToString(cx, argv[0]);
	  if (!strtype) {
		JS_ReportError(cx,"The second argument to file.open must be a string.");
		return JS_FALSE;
	  }
	  ctype = JS_GetStringBytes(strtype);
	} else
	  ctype = "";

	strcpy(debug,"First argument to open was ");
	strcat(debug,ctype);
	OutputDebugString(debug);

	if (!strcmp(ctype,"ascii"))
		type=ASCII;
	else
	if (!strcmp(ctype,"utf"))
		type=UTF;
	else
	if (!strcmp(ctype,"unicode"))
		type=UNICODE;
	else
		type=UTF;


	mask=0;
	mask|=(file_has_option(mode,"readOnly"))?PR_RDONLY:0;
	mask|=(file_has_option(mode,"writeOnly"))?PR_WRONLY:0;
	mask|=(file_has_option(mode,"readWrite"))?PR_RDWR:0;
	mask|=(file_has_option(mode,"append"))?PR_APPEND:0;
	mask|=(file_has_option(mode,"create"))?PR_CREATE_FILE:0;
	mask|=(file_has_option(mode,"truncate"))?PR_TRUNCATE:0;
	mask|=(file_has_option(mode,"replace"))?PR_TRUNCATE:0;

	file->autoflush|=(file_has_option(mode,"autoflush"));

	if ((mask&(PR_RDONLY|PR_WRONLY))==0)
		mask|=PR_RDWR;

	handle=PR_Open(file->path, mask, 0644); /* what about the permissions?? Java seems to ignore the problem.. */

	file->type=type;
	file->mode=mask;
	file->handle=handle;
    file->randomAccess=(type!=UTF);

	resetBuffers(file);

	if (handle==NULL) {
		*rval=BOOLEAN_TO_JSVAL(JS_FALSE);
	} else {
		file->opened=JS_TRUE;
		*rval=BOOLEAN_TO_JSVAL(JS_TRUE);
	}

	return JS_TRUE;
}

static JSBool
file_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile	*file;
	PRStatus status;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (file->handle==NULL) {
		*rval=JSVAL_FALSE;
		return JS_TRUE;
	}

	status=PR_Close(file->handle);
	if (status!=0) {
	  *rval=JSVAL_FALSE;
	  return JS_TRUE;
	}
	file->handle=NULL;
	file->opened=JS_FALSE;
	resetBuffers(file);
	*rval=JSVAL_TRUE;

	return JS_TRUE;
}


static JSBool
file_exists(JSFile*file)
{
	PRStatus status;
	status = PR_Access(file->path, PR_ACCESS_EXISTS);
	return (status==PR_SUCCESS);
}

static JSBool
file_canRead(JSFile*file)
{
	PRStatus status;
	status = PR_Access(file->path, PR_ACCESS_READ_OK);
	return (status==PR_SUCCESS);
}

static JSBool
file_canWrite(JSFile*file)
{
	PRStatus status;
	status = PR_Access(file->path, PR_ACCESS_WRITE_OK);
	return (status==PR_SUCCESS);
}

static JSBool
file_isFile(JSFile*file)
{
	
	PRStatus status;
	PRFileInfo info;
	if (file->opened) {
	  status=PR_GetOpenFileInfo(file->handle,&info);
	} else {
		  status=PR_GetFileInfo(file->path,&info);
	}
	if (status==PR_FAILURE)
		return JS_FALSE;
	return (info.type==PR_FILE_FILE);
}


static JSBool
file_isDirectory(JSFile*file)
{
	PRStatus status;
	PRFileInfo info;
	if (file->opened) {
	  status=PR_GetOpenFileInfo(file->handle,&info);
	} else {
		  status=PR_GetFileInfo(file->path,&info);
	}
	if (status==PR_FAILURE)
		return JS_FALSE;
	return (info.type==PR_FILE_DIRECTORY);
}

static JSBool
file_remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile	*file;
	PRStatus status;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (file_isDirectory(file)) {
	  status = PR_RmDir(file->path);
	} else {
  	  status = PR_Delete(file->path);
	}
	if (status==PR_SUCCESS) {
		file->opened=JS_FALSE; /* I'm not sure I should do that.. */
		*rval = JSVAL_TRUE;
	} else {
		*rval = JSVAL_FALSE;
	}
	return JS_TRUE;
}

static JSBool
file_copyTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile	*file;
	PRStatus status;
	char*str;
	PRFileDesc* handle;
	PRFileInfo info;
	char* buffer;
	unsigned long int count;

	if (argc!=1)
	return JS_FALSE; 

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	str=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

	if (file->opened) {
		JS_ReportError(cx,"cannot copy an opened file.");
		*rval = JSVAL_FALSE;
		return JS_TRUE;
	}

	handle=PR_Open(str, PR_WRONLY|PR_CREATE_FILE|PR_TRUNCATE, 0644 );
	file->handle=PR_Open(file->path, PR_RDONLY, 0644);

    status=PR_GetOpenFileInfo(file->handle,&info);
	if (status==PR_FAILURE) {
	  JS_ReportError(cx,"Cannot access this file informations");
	  *rval = JSVAL_FALSE;
	  return JS_TRUE;
	}

	buffer=JS_malloc(cx,info.size);
	count=PR_Read(file->handle,buffer,info.size);
	if (count!=info.size) {
		JS_free(cx,buffer);
		PR_Close(file->handle);
		PR_Close(handle);
		JS_ReportError(cx,"An error occured while attempting to read a file to copy.");
		*rval = JSVAL_FALSE;
		return JS_TRUE;
	}
	count=PR_Write(handle,buffer,info.size);
	if (count!=info.size) {
		JS_free(cx,buffer);
		PR_Close(file->handle);
		PR_Close(handle);
		JS_ReportError(cx,"An error occured while attempting to copy into a file.");
		*rval=JSVAL_FALSE;
		return JS_TRUE;
	}
	JS_free(cx,buffer);
	PR_Close(file->handle);
	PR_Close(handle);

	*rval = JSVAL_TRUE;
	return JS_TRUE;
}

static JSBool
file_renameTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile	*file;
	PRStatus status;
	char*str;

	if (argc<1) {
	  JS_ReportError(cx,"file.renameTo expects one argument.");
	  return JS_FALSE; 
	}

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	str=JS_GetStringBytes(JS_ValueToString(cx, argv[0]));
	status = PR_Rename(file->path,str);
	if (status==PR_FAILURE)
	  *rval = JSVAL_FALSE;
	else 
	  *rval = JSVAL_TRUE;

	return JS_TRUE;
	
}

static JSBool
file_flush(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile	*file;
	PRStatus status;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (!file->opened) {
		JS_ReportError(cx,"Cannot flush a non-opened file.");
		return JS_FALSE;
	}
	status = PR_Sync(file->handle);
	if (status==PR_FAILURE)
	  *rval = JSVAL_FALSE;
    else
	  *rval = JSVAL_TRUE;

	return JS_TRUE;
}


/* write(s) methods..
	First, check if the file is open.
	If not, try to open it with default values.
	  ( here, append mode.. )
*/

static JSBool
file_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN i;
	JSFile	*file;
	JSString *str;
	int count;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (!file->opened) {
		JSString *type,*mask;
		jsval v[2];
		jsval rval;
		JSBool b;
		type= JS_NewStringCopyZ(cx, "utf");
		mask= JS_NewStringCopyZ(cx, "create,append,writeOnly");
		v[0]=STRING_TO_JSVAL(type);
		v[1]=STRING_TO_JSVAL(mask);
		b=file_open(cx,obj,2,v,&rval);
		if (!file->opened) {
			JS_ReportError(cx, "Cannot open file for writing.\n");
			return JS_FALSE;
		}
	}
	
	for (i=0;i<argc;i++) {
        str=JS_ValueToString(cx, argv[i]);
		count=JS_FileWrite(file,JS_GetStringChars(str),JS_GetStringLength(str),file->type);
		if (count==-1) 
		  return JS_TRUE;
	}

	*rval = JSVAL_TRUE;
	return JS_TRUE;
}

static JSBool
file_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile		*file;
	JSString	*str;
	JSBool		b;
	int			count;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	b=file_write(cx,obj,argc,argv,rval);
	if (!b)
	return JS_FALSE;

	str=JS_NewStringCopyZ(cx,LINEBREAK);
	count=JS_FileWrite(file,JS_GetStringChars(str),JS_GetStringLength(str),file->type);

	if (file->autoflush)
		file_flush(cx,obj,0,NULL,NULL);

	*rval= BOOLEAN_TO_JSVAL(count>=0);
	return JS_TRUE;
}

static JSBool
file_writeAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile		*file;
	jsint		iter;
	jsint		limit;
	JSObject	*array;
	JSObject	*elem;
	jsval		elemval;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (argc<1) {
		JS_ReportError(cx,"Wrong number of argument passed to writeAll");
		return JS_FALSE;
	}

	if (!JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[0]))) {
		JS_ReportError(cx,"writeAll expects an array for argument.");
		return JS_FALSE;
	}
	array=JSVAL_TO_OBJECT(argv[0]);
    if (!JS_GetArrayLength(cx, array, &limit)) {
		return JS_FALSE; /* not supposed to happen */
	}
	

	for (iter=0; iter<limit; iter++) {
      if (!JS_GetElement(cx, array, iter, &elemval))
		  return JS_FALSE;
	  elem=JSVAL_TO_OBJECT(elemval);
	  file_writeln(cx,obj,1,&elemval,rval);
	}

	*rval = JSVAL_TRUE;
	return JS_TRUE;
}

static JSBool
file_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile		*file;
	JSString	*str;
	int32		want,count;
	jschar		*buf;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (!file->opened) {
		JSString *type,*mask;
		jsval v[2];
		jsval rval;
		JSBool b;
		type= JS_NewStringCopyZ(cx, "utf");
		mask= JS_NewStringCopyZ(cx, "readOnly");
		v[0]=STRING_TO_JSVAL(type);
		v[1]=STRING_TO_JSVAL(mask);
		b=file_open(cx,obj,2,v,&rval);
		if (!file->opened) {
			JS_ReportError(cx, "Cannot open file \n");
			return JS_FALSE;
		}
	}
	
	if (!JS_ValueToInt32(cx, argv[0], &want))
	return JS_FALSE;

	want=(want>262144)?262144:want; /* arbitrary size limitation */
									
	buf = JS_malloc(cx,want*sizeof buf[0]); 
	if (!buf)
	return JS_FALSE;

	count= JS_FileRead(file,buf,want,file->type);
	if (count>0) {
	  str = JS_NewUCStringCopyN(cx, buf, count);
	  *rval=STRING_TO_JSVAL(str);
	  JS_free(cx,buf);
	} else {
	  JS_free(cx,buf);
	  JS_ReportError(cx,"Cannot read file %s.",file->path);
	  return JS_FALSE;
	}

	return JS_TRUE;
}


static JSBool
file_readln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile		*file;
	JSString	*str;
	jschar		*buf;
	int32		offset;
	intN		room;
	jschar		data,data2;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (!file->opened) {
		JSString *type,*mask;
		jsval v[2];
		jsval rval;
		JSBool b;
		type= JS_NewStringCopyZ(cx, "utf");
		mask= JS_NewStringCopyZ(cx, "readOnly");
		v[0]=STRING_TO_JSVAL(type);
		v[1]=STRING_TO_JSVAL(mask);
		b=file_open(cx,obj,2,v,&rval);
		if (!file->opened) {
			JS_ReportError(cx, "Cannot open file \n");
			return JS_FALSE;
		}
	}

	if (!file->linebuffer) {
		buf = malloc(128*(sizeof data));
		if (!buf) {
			JS_ReportOutOfMemory(cx);
			return JS_FALSE;
		}			
		file->linebuffer=JS_NewUCString(cx,buf,128);
	}
	room=JS_GetStringLength(file->linebuffer);
	offset=0;

	/* XXX TEST ME!! */
    for(;;) {

		if (!JS_FileRead(file,&data,1,file->type))
			goto loop;
	    switch (data) {
		  case '\n' : 
			  goto loop;
		  case '\r' :
			  if (!JS_FileRead(file,&data2,1,file->type))
				  goto loop;
			  if (data2!='\n') { /* we read one char to far. buffer it. */
				  file->charBuffer=data2;
			      file->charBufferUsed=JS_TRUE;
			  }
			  goto loop;
		  default:
			  if (--room < 0) {
				  buf = malloc((offset+ 128)*sizeof data);
				  if (!buf) {
					  JS_ReportOutOfMemory(cx);
					  return JS_FALSE;
				  }
				  room = 127;
				  memcpy(buf,JS_GetStringChars(file->linebuffer),
					  JS_GetStringLength(file->linebuffer));
				  /* what follows may not be the cleanest way. */
				  file->linebuffer->chars = buf;
				  file->linebuffer->length= offset+128;
			  }
			  file->linebuffer->chars[offset++] = data;
			  break;
		}
	}
loop:
	file->linebuffer->chars[offset]=0;
	str = JS_NewUCStringCopyN(cx,JS_GetStringChars(file->linebuffer),
		offset);
	*rval=STRING_TO_JSVAL(str);
	return JS_TRUE;
}

static JSBool
file_readAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile		*file;
	PRFileInfo	info;
	PRStatus	status;
	JSObject	*array;
	int			len;
	jsval		line;
	JSBool		ok=JS_TRUE;


	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	array=JS_NewArrayObject(cx, 0, NULL);
    *rval = OBJECT_TO_JSVAL(array);
	len = 0;


	if (!file->opened) {
		JSString *type,*mask;
		jsval v[2];
		jsval rval;
		JSBool b;
		type= JS_NewStringCopyZ(cx, "utf");
		mask= JS_NewStringCopyZ(cx, "readOnly");
		v[0]=STRING_TO_JSVAL(type);
		v[1]=STRING_TO_JSVAL(mask);
		b=file_open(cx,obj,2,v,&rval);
		if (!file->opened) {
			JS_ReportError(cx, "Cannot open file \n");
			return JS_FALSE;
		}
	}


    status=PR_GetOpenFileInfo(file->handle,&info);
	if (status==PR_FAILURE)
	return JS_FALSE;
	while (ok&&(info.size>PR_Seek(file->handle, 0, PR_SEEK_CUR))) {
		ok=file_readln(cx,obj,0,NULL,&line);
	    JS_SetElement(cx, array, len, &line);
		len++;
	}

}

static JSBool
file_skip(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSFile		*file;
	int32		toskip,count;

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (!file->opened) {
		JSString *type,*mask;
		jsval v[2];
		jsval rval;
		JSBool b;
		type= JS_NewStringCopyZ(cx, "utf");
		mask= JS_NewStringCopyZ(cx, "readOnly");
		v[0]=STRING_TO_JSVAL(type);
		v[1]=STRING_TO_JSVAL(mask);
		b=file_open(cx,obj,2,v,&rval);
		if (!file->opened) {
			JS_ReportError(cx, "Cannot open file \n");
			return JS_FALSE;
		}
	}
	
	if (!JS_ValueToInt32(cx, argv[0], &toskip))
	return JS_FALSE;

	count= JS_FileSkip(file,toskip,file->type);
}

static JSBool
file_list(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) 
{
	PRDir *dir;
	PRDirEntry *entry;
	JSFile * file;
	JSObject *array;
	JSObject *each;
	JSFile *eachObj;
	jsint len;
	jsval v;
	JSRegExp *re = NULL;
	JSFunction *func = NULL;
	JSString * tmp;
	size_t index;
	jsval args[1];
	char* aux;


	if (argc>0) {
  	    if (JSVAL_IS_REGEXP(cx,argv[0])) {
		  re = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
		} else
		if (JSVAL_IS_FUNCTION(cx,argv[0])) {
		  func = JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
		}
    }

	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	if (!file_isDirectory(file)) {
		*rval = JSVAL_FALSE;
		return JS_TRUE; /* or return an empty array? or do a readAll?  */
	}
	dir=PR_OpenDir(file->path);
	/* Create JSArray here.. */
	array=JS_NewArrayObject(cx, 0, NULL);
    *rval = OBJECT_TO_JSVAL(array);
	len=0;

	entry=NULL;
	if (dir!=NULL)
	  entry=PR_ReadDir(dir,PR_SKIP_BOTH);
	while (entry!=NULL) {
		/* First, check if we have a filter */
		if (re!=NULL) {
			tmp=JS_NewStringCopyZ(cx,entry->name);
			index=0;
			js_ExecuteRegExp(cx, re, tmp, &index, JS_TRUE, &v);
			if (v==JSVAL_NULL) {
				entry=PR_ReadDir(dir,PR_SKIP_BOTH);
				continue;
			}

		}
		if (func!=NULL) {
			tmp=JS_NewStringCopyZ(cx,entry->name);
			args[0]=STRING_TO_JSVAL(tmp);
			JS_CallFunction(cx, obj, func, 1, args, &v);
			if (v==JSVAL_FALSE) {
				entry=PR_ReadDir(dir,PR_SKIP_BOTH);
				continue;
			}
		}
		/* add entry->name to the Array.. */
		aux = (char*)malloc(strlen(file->path)+strlen(entry->name)+1);
		if (!aux) {
			JS_ReportOutOfMemory(cx);
			return JS_FALSE;
		}
		strcpy(aux,file->path);
		strcat(aux,entry->name);
	    each=NewFileObject(cx, aux);
 	    if (!each)
		return JS_FALSE;
		eachObj = JS_GetInstancePrivate(cx, each, &file_class, NULL);
		if (!eachObj)
		return JS_FALSE;
		v=OBJECT_TO_JSVAL(each);
	    JS_SetElement(cx, array, len, &v);
		JS_SetProperty(cx, array, entry->name, &v); /* accessible by name.. make sense I think.. */
		len++;
		entry=PR_ReadDir(dir,PR_SKIP_BOTH);
	}
	PR_CloseDir(dir);

	return JS_TRUE;
}

static JSBool
file_mkdir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) 
{
	JSFile * file;
	PRStatus status;
	int index;
	char * str;
	JSObject*newobj;


	file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_FALSE;

	index=strlen(file->path)-1;
	while ((index>0)&&((file->path[index]=='/')||(file->path[index]=='\\'))) index--;
	while ((index>0)&&(file->path[index]!='/')&&(file->path[index]!='\\')) index--;

	if (index>0) {
	  str=(char*)JS_malloc(cx,index+1);
	  if (!str) {
		JS_ReportOutOfMemory(cx);
		return JS_FALSE;
	  }
	  strncpy(str,file->path,index);
	  str[index]='\0';
	  newobj=NewFileObject(cx, str);
	  if (!file_mkdir(cx,newobj,argc,argv,rval))
		return JS_FALSE;
	}

	status=PR_MkDir(file->path, 0755);
	if (status==PR_FAILURE) 
		*rval=JSVAL_FALSE;
	else
		*rval=JSVAL_TRUE;
	return JS_TRUE;
}

static JSFunctionSpec file_functions[] = {
	{ js_toString_str,	file_toString, 0},
	{ "open",			file_open, 0},
	{ "close",			file_close, 0},
	{ "remove",			file_remove, 0},
	{ "copyTo",			file_copyTo, 0},
	{ "renameTo",		file_renameTo, 0},
	{ "flush",			file_flush, 0},
	{ "skip",			file_skip, 0},
	{ "read",			file_read, 0},
	{ "readln",			file_readln, 0},
	{ "readAll",		file_readAll, 0},
	{ "write",			file_write, 0},
	{ "writeln",		file_writeln, 0},
	{ "writeAll",		file_writeAll, 0},
	{ "list",			file_list, 0},
	{ "mkdir",			file_mkdir, 0},
	{0}
};

enum file_tinyid {
	FILE_LENGTH				= -2,
	FILE_PARENT				= -3,
	FILE_PATH				= -4,
	FILE_NAME				= -5,
	FILE_ISDIR				= -6,
	FILE_ISFILE				= -7,
	FILE_EXISTS				= -8,
	FILE_CANREAD			= -9,
	FILE_CANWRITE			= -10,
	FILE_OPENED				= -11,
	FILE_TYPE				= -12,
	FILE_MODE				= -13,
	FILE_CREATED			= -14,
	FILE_MODIFIED			= -15,
	FILE_SIZE				= -16,
	FILE_RANDOMACCESS		= -17,
	FILE_POSITION			= -18

};

static JSPropertySpec file_props[] = {
	{"length",		FILE_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY }, 
	{"parent",		FILE_PARENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"path",		FILE_PATH,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{"name",		FILE_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{"isDirectory",	FILE_ISDIR,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{"isFile",		FILE_ISFILE,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"exists",		FILE_EXISTS,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"canRead",		FILE_CANREAD,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"canWrite",	FILE_CANWRITE,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"opened",		FILE_OPENED,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"type",		FILE_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{"mode",		FILE_MODE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{"creationTime",FILE_CREATED,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"lastModified",FILE_MODIFIED,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"size",		FILE_SIZE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{"randomAccess",FILE_RANDOMACCESS,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{"position",	FILE_POSITION,	JSPROP_ENUMERATE }, /* . */
	{0}
};

enum fileclass_tinyid {
	FILE_STDIN	,
	FILE_STDOUT	,
	FILE_STDERR	
};

static JSPropertySpec file_static_props[] = {
	{"stdin",			FILE_STDIN,			JSPROP_ENUMERATE },
	{"stdout",			FILE_STDOUT,		JSPROP_ENUMERATE },
	{"stderr",			FILE_STDERR,		JSPROP_ENUMERATE },
    {0}
};

#define FILE_SEEK_SET 0x1
#define FILE_SEEK_CUR 0x2
#define FILE_SEEK_END 0x3


static JSConstDoubleSpec file_constants[] = {
	{FILE_SEEK_SET,		"SEEK_SET"},
	{FILE_SEEK_CUR,		"SEEK_CUR"},
	{FILE_SEEK_END,		"SEEK_END"},
	{0}
};

JSBool PR_CALLBACK
file_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	JSFile * file;
	char *str;
	int tiny;
	PRStatus status;
	PRFileInfo info;
    PRExplodedTime  expandedTime;
	JSObject * tmp;
	int aux,index;
	JSObject * newobj;
	JSString * newstring;
	JSBool flag;

	tiny=JSVAL_TO_INT(id);
	file= JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file)
	return JS_TRUE;

	switch (tiny) {
	case FILE_LENGTH: /* Ok, this works only on direct access files. use .size on other files */

		if (!file->randomAccess)
			break;
		if (!file->opened)
			break;
 	    status=PR_GetOpenFileInfo(file->handle,&info);
		if (status==PR_FAILURE)
		  break;
		switch (file->type) {
		case UNICODE:
			*vp = INT_TO_JSVAL(info.size>>1);
			break;
		case ASCII:
		default:
			*vp = INT_TO_JSVAL(info.size);
			break;			
		}

		break;
	case FILE_PARENT:
		/* does .. always refer to the parent directory? */
		/* right thing is probably to check for '/' and strip.. */
		/* This code is going to blow if there's a '\' in the name. */
		index=strlen(file->path)-1;
		while ((index>0)&&((file->path[index]=='/')||(file->path[index]=='\\'))) index--;
		while ((index>0)&&(file->path[index]!='/')&&(file->path[index]!='\\')) index--;

		if (index>0) {
		  str=(char*)JS_malloc(cx,index+1);
		  if (!str) {
			JS_ReportOutOfMemory(cx);
			return JS_FALSE;
		  }
		  strncpy(str,file->path,index);
		  str[index]='\0';
		  newobj=NewFileObject(cx, str);
		} else
		  newobj=obj;
		*vp = OBJECT_TO_JSVAL(newobj);
		break;
	case FILE_PATH: /* I wish I'd have a cross platform way to canonize this.. */
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,file->path));
		break;
	case FILE_NAME:
		/* String manipulation here, or a prio function already here? */
		/* String manipulation all the way.. XXX Test me on Mac!! */
		index=strlen(file->path)-1;
		while ((index>0)&&((file->path[index]=='/')||(file->path[index]=='\\'))) index--;
		aux=index;
		while ((index>0)&&(file->path[index]!='/')&&(file->path[index]!='\\')) index--;
		newstring=JS_NewStringCopyN(cx, &file->path[index+1], aux-index);
		*vp = STRING_TO_JSVAL(newstring);
		break;
	case FILE_ISDIR:
		*vp = BOOLEAN_TO_JSVAL(file_isDirectory(file));
		break;
	case FILE_ISFILE:
		*vp = BOOLEAN_TO_JSVAL(file_isFile(file));
		break;
	case FILE_EXISTS:
		*vp = BOOLEAN_TO_JSVAL(file_exists(file));
		break;
	case FILE_CANREAD:
		*vp = BOOLEAN_TO_JSVAL(file_canRead(file));
		break;
	case FILE_CANWRITE:
		*vp = BOOLEAN_TO_JSVAL(file_canWrite(file));
		break;
	case FILE_OPENED:
		*vp = BOOLEAN_TO_JSVAL(file->opened);
		break;
	case FILE_TYPE:
		switch (file->type) {
		case ASCII: 
			*vp = STRING_TO_JSVAL(JS_NewString(cx,"ascii",5));
			break;
		case UTF:
			*vp = STRING_TO_JSVAL(JS_NewString(cx,"utf",3));
			break;
		case UNICODE:
			*vp = STRING_TO_JSVAL(JS_NewString(cx,"unicode",7));
			break;
		default:
			; /* time to panic, or to do nothing.. */
		}
		break;
	case FILE_MODE:
		str=(char*)JS_malloc(cx,200); /* Big enough to contain all the modes.. */
		str[0]='\0';
		flag=JS_FALSE;

		if ((file->mode&PR_RDONLY)==PR_RDONLY) {
			if (flag) strcat(str,",");
			strcat(str,"readOnly");
			flag=JS_TRUE;
		}
		if ((file->mode&PR_WRONLY)==PR_WRONLY) {
			if (flag) strcat(str,",");
			strcat(str,"writeOnly");
			flag=JS_TRUE;
		}
		if ((file->mode&PR_RDWR)==PR_RDWR) {
			if (flag) strcat(str,",");
			strcat(str,"readWrite");
			flag=JS_TRUE;
		}
		if ((file->mode&PR_APPEND)==PR_APPEND) {
			if (flag) strcat(str,",");
			strcat(str,"append");
			flag=JS_TRUE;
		}
		if ((file->mode&PR_CREATE_FILE)==PR_CREATE_FILE) {
			if (flag) strcat(str,",");
			strcat(str,"create");
			flag=JS_TRUE;
		}
		if ((file->mode&PR_TRUNCATE)==PR_TRUNCATE) {
			if (flag) strcat(str,",");
			strcat(str,"replace");
			flag=JS_TRUE;
		}
		if (file->autoflush) {
			if (flag) strcat(str,",");
			strcat(str,"autoflush");
			flag=JS_TRUE;
		}
		newstring=JS_NewStringCopyZ(cx, str);
		*vp = STRING_TO_JSVAL(newstring);
		JS_free(cx, str);
		break;
	case FILE_CREATED:
		if (file->opened) {
		  status=PR_GetOpenFileInfo(file->handle,&info);
		} else {
  		  status=PR_GetFileInfo(file->path,&info);
		}
		if (status==PR_FAILURE)
		  break;
		
        PR_ExplodeTime(info.creationTime, PR_LocalTimeParameters, &expandedTime);
		tmp=js_NewDateObject(cx,	expandedTime.tm_year, 
									expandedTime.tm_month,
									expandedTime.tm_mday,
						            expandedTime.tm_hour,
									expandedTime.tm_min,
									expandedTime.tm_sec );
		*vp = OBJECT_TO_JSVAL(tmp);
		break;
	case FILE_MODIFIED:
		if (file->opened) {
		  status=PR_GetOpenFileInfo(file->handle,&info);
		} else {
  		  status=PR_GetFileInfo(file->path,&info);
		}
		if (status==PR_FAILURE)
		  break;
		
        PR_ExplodeTime(info.modifyTime, PR_LocalTimeParameters, &expandedTime);
		tmp=js_NewDateObject(cx,	expandedTime.tm_year, 
									expandedTime.tm_month,
									expandedTime.tm_mday,
						            expandedTime.tm_hour,
									expandedTime.tm_min,
									expandedTime.tm_sec );
		*vp = OBJECT_TO_JSVAL(tmp);
		break;
	case FILE_SIZE:
		if (file_isDirectory(file)) {
		  PRDir *dir;
		  PRDirEntry *entry;
		  int count;

		  dir=PR_OpenDir(file->path);
		  if (dir!=NULL)
		    entry=PR_ReadDir(dir,PR_SKIP_BOTH);
		  else
			break;

		  count=0;
		  while (entry!=NULL) {
			count++;
			entry=PR_ReadDir(dir,PR_SKIP_BOTH);
		  }
		  PR_CloseDir(dir);
		  *vp = INT_TO_JSVAL(count);
		}

		if (file->opened) {
		  status=PR_GetOpenFileInfo(file->handle,&info);
		} else {
  		  status=PR_GetFileInfo(file->path,&info);
		}
		if (status!=PR_FAILURE)		
		  *vp = INT_TO_JSVAL(info.size);
		break;
	case FILE_RANDOMACCESS:
		*vp = BOOLEAN_TO_JSVAL(file->randomAccess);
		break;
	case FILE_POSITION:
		if (file->opened) {
		  *vp = INT_TO_JSVAL(PR_Seek(file->handle, 0, PR_SEEK_CUR));
		} else {
		  *vp = JSVAL_VOID;
		}
		break;
	default:
		break;
	}
	return JS_TRUE;
}

JSBool PR_CALLBACK
file_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFile *file;
    jsint slot;
	int32 offset;
	int32 count;

    file = JS_GetInstancePrivate(cx, obj, &file_class, NULL);
	if (!file) {
	    return JS_TRUE;
    }

    if (JSVAL_IS_STRING(id)) {
		return JS_TRUE;
    }
	
    slot = JSVAL_TO_INT(id);

    switch (slot) {
    case FILE_POSITION: /* XXX We need to make this depens on the can of data we're using */
		if (file->randomAccess) {
		  offset=JSVAL_TO_INT(*vp);
		  count=PR_Seek(file->handle, offset, PR_SEEK_SET);
          resetBuffers(file);
		  *vp = INT_TO_JSVAL(count);
		}
		break;
	default:
		break;
	}

	return JS_TRUE;
}

static JSClass file_class = {
	"File", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  file_getProperty,  file_setProperty,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSFile*
file_constructor(JSContext *cx, JSObject*obj, char *bytes)
{
	JSFile		*file;
	char x;
	char *buf;

	file=JS_malloc(cx, sizeof *file);
	if (!file)
		return NULL;
	memset(file, 0 , sizeof *file);

	if (bytes[0]=='[') { /* special file.. */
		file->path="";
	} else {
		file->path=bytes;
	}
	if (file_isDirectory(file)) {
		x=file->path[strlen(file->path)-1];
		if ((x!='/')&&(x!='\\')) {
			buf=malloc(strlen(file->path)+2);
			strcpy(buf,file->path);
			strcat(buf,"/");
			if (bytes==file->path)
				free(bytes);
			file->path=buf;
		}
	}
	file->opened=JS_FALSE;
	file->handle=NULL;
	file->nbBytesInBuf=0;
	file->charBufferUsed=JS_FALSE;
	file->randomAccess=JS_TRUE; /* innocent until proven guilty */


    if (!JS_SetPrivate(cx, obj, file)) {
		JS_free(cx, file);
		return NULL;
    }

	return file;
}

static JSObject*
NewFileObject(JSContext *cx, char*bytes)
{
	JSObject	*f;
	JSFile		*file;
	f=JS_NewObject(cx, &file_class, NULL, NULL);
	if (!f)
	return NULL;

	file=file_constructor(cx,f,bytes);
	if (!file)
	return NULL;

	return f;
}

static JSObject*
NewFileObjectCopyZ(JSContext *cx, char*bytes)
{
	JSObject	*f;
	JSFile		*file;
	char		*copybytes;
	f=JS_NewObject(cx, &file_class, NULL, NULL);
	if (!f)
	return NULL;

	copybytes=malloc(strlen(bytes)+1);
	strcpy(copybytes,bytes);

	file=file_constructor(cx,f,copybytes);
	if (!file)
	return NULL;

	return f;
}

static JSBool
File(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {

	JSString *str;
	JSFile   *file;

/*
    if (obj->map->clasp != &file_class) {
		*rval=JSVAL_VOID;
		return JS_TRUE;
	}
*/

	if (argc==0) {
		JS_ReportError(cx,"Cannot create a File object without a file name.");
		return JS_FALSE;
	}

	str=JS_ValueToString(cx,argv[0]);
	if (!str) {
		JS_ReportError(cx,"The argument to the File constructor must be a string.");
		return JS_FALSE;
	}

    file = file_constructor(cx,obj,JS_GetStringBytes(str));
	if (!file) {
		*rval=JSVAL_VOID;
		return JS_TRUE;
	}

	return JS_TRUE;
}

JSObject* js_InitFileClass(JSContext *cx, JSObject* obj) {
	JSObject * file, *ctor;
	JSObject	*afile;
	JSFile		*fileObj;
	jsval		vp;

	
	file = JS_InitClass(cx, obj, NULL, &file_class, File, 1,
		file_props, file_functions, NULL, NULL);

	if (!file)
		return NULL;

	ctor = JS_GetConstructor(cx, file);
	if (!ctor) {
		OutputDebugString("File.constructor returned null...\n");
		return NULL;
	}

	afile=NewFileObject(cx,"[ stdin ]");
	if (!afile)
	return NULL;
	fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
	if (!fileObj)
	return NULL;
	fileObj->handle = PR_STDIN;
	fileObj->opened = JS_TRUE;
    fileObj->randomAccess=JS_FALSE;
	vp=OBJECT_TO_JSVAL(afile);
	JS_SetProperty(cx, ctor, "stdin", &vp);
	JS_SetProperty(cx, obj, "stdin", &vp);

	afile=NewFileObject(cx,"[ stdout ]");
	if (!afile)
	return NULL;
	fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
	if (!fileObj)
	return NULL;
	fileObj->handle = PR_STDOUT;
	fileObj->opened = JS_TRUE;
    fileObj->randomAccess=JS_FALSE;
	vp=OBJECT_TO_JSVAL(afile);
	JS_SetProperty(cx, ctor, "stdout", &vp);
	JS_SetProperty(cx, obj, "stdout", &vp);

	afile=NewFileObject(cx,"[ stderr ]");
	if (!afile)
	return NULL;
	fileObj = JS_GetInstancePrivate(cx, afile, &file_class, NULL);
	if (!fileObj)
	return NULL;
	fileObj->handle = PR_STDERR;
	fileObj->opened = JS_TRUE;
    fileObj->randomAccess=JS_FALSE;
	vp=OBJECT_TO_JSVAL(afile);
	JS_SetProperty(cx, ctor, "stderr", &vp);
	JS_SetProperty(cx, obj, "stderr", &vp);

	if (!JS_DefineConstDoubles(cx, ctor, file_constants))
	return NULL;

	return file;
}
