/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License 
 * Version 1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for 
 * the specific language governing rights and limitations under the License.
 *
 * Contributors:
 *    Frank Mitchell (frank.mitchell@sun.com)
 */

/*
 * A utility for creating static Java proxies to XPCOM objects from XPT files
 */ 

#include "xpt_xdr.h"
#include <stdio.h>
#include <ctype.h>
#ifdef XP_MAC
#include <stat.h>
#include <StandardFile.h>
#include "FullPath.h"
#else
#include <sys/stat.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "prprf.h"

#define BASE_INDENT 3

static char *type_array[18] = {"byte", "short", "int", "long",
                               "byte", "short", "int", "long",
                               "float", "double", "boolean", "char",
                               "char", "void", "reserved", "reserved",
                               "reserved", "reserved"};

static char *rtype_array[20] = {"Byte", "Short", "Integer", "Long",
                                "Byte", "Short", "Integer", "Long",
                                "Float", "Double", "Boolean", "Character",
                                "Character", "Object", "nsID", "String",
                                "String", "String", "nsISupports", 
                                "nsISupports"};


PRBool
GenproxyClass(FILE *out,
                  XPTCursor *cursor,
                  XPTInterfaceDirectoryEntry *ide, 
                  XPTHeader *header);

PRBool GenproxyMethodPrototype(FILE *out, 
                               XPTHeader *header, 
                               XPTMethodDescriptor *md);

PRBool GenproxyMethodBody(FILE *out, 
                          XPTHeader *header, 
                          XPTMethodDescriptor *md, 
                          int methodIndex,
                          const char *iidName);

PRBool
GenproxyGetStringForType(XPTHeader *header, XPTTypeDescriptor *td,
                         char **type_string);

PRBool
GenproxyGetStringForRefType(XPTHeader *header, XPTTypeDescriptor *td,
                            char **type_string);

static void
genproxy_usage(char *argv[]) {
    fprintf(stdout, "Usage: %s [-v] <filename.xpt>\n"
            "       -v verbose mode\n", argv[0]);
}

#ifdef XP_MAC

static int mac_get_args(char*** argv)
{
	static char* args[] = { "genproxy", NULL, NULL };
	static StandardFileReply reply;
	
	*argv = args;
	
	printf("choose an .xpt file to dump.\n");
	
	StandardGetFile(NULL, 0, NULL, &reply);
	if (reply.sfGood && !reply.sfIsFolder) {
		short len = 0;
		Handle fullPath = NULL;
		if (FSpGetFullPath(&reply.sfFile, &len, &fullPath) == noErr && fullPath != NULL) {
			char* path = args[1] = XPT_MALLOC(1 + len);
			BlockMoveData(*fullPath, path, len);
			path[len] = '\0';
			DisposeHandle(fullPath);
			return 2;
		}
	}
	
	return 1;
}

#ifdef XPIDL_PLUGIN
#define main xptdump_main
int xptdump_main(int argc, char *argv[]);
#endif

#endif 


#if defined(XP_MAC) && defined(XPIDL_PLUGIN)

#define get_file_length mac_get_file_length
extern size_t mac_get_file_length(const char* filename);

#else /* !(XP_MAC && XPIDL_PLUGIN) */

static size_t get_file_length(const char* filename)
{
    struct stat file_stat;
    if (stat(filename, &file_stat) != 0) {
        perror("FAILED: get_file_length");
        exit(1);
    }
    return file_stat.st_size;
}

#endif /* !(XP_MAC && XPIDL_PLUGIN) */

int 
main(int argc, char **argv)
{
    PRBool verbose_mode = PR_FALSE;
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    size_t flen;
    char *name;
    char *whole;
    FILE *in;
    FILE *out;
    int i;

#ifdef XP_MAC
	if (argc == 0 || argv == NULL)
	argc = mac_get_args(&argv);
#endif

    switch (argc) {
    case 2:
        if (argv[1][0] == '-') {
            genproxy_usage(argv);
            return 1;
        }
        name = argv[1];
        flen = get_file_length(name);
        in = fopen(name, "rb");
        break;
    case 3:
        verbose_mode = PR_TRUE;
        if (argv[1][0] != '-' || argv[1][1] != 'v') {
            genproxy_usage(argv);
            return 1;
        }
        name = argv[2];
        flen = get_file_length(name);
        in = fopen(name, "rb");
        break;
    default:
        genproxy_usage(argv);
        return 1;
    }

    if (!in) {
        perror("FAILED: fopen");
        return 1;
    }

    whole = malloc(flen);
    if (!whole) {
        perror("FAILED: malloc for whole");
        return 1;
    }

    if (flen > 0) {
        size_t rv = fread(whole, 1, flen, in);
        if (rv < 0) {
            perror("FAILED: reading typelib file");
            return 1;
        }
        if (rv < flen) {
            fprintf(stderr, "short read (%d vs %d)! ouch!\n", rv, flen);
            return 1;
        }
        if (ferror(in) != 0 || fclose(in) != 0)
            perror("FAILED: Unable to read typelib file.\n");
        
        state = XPT_NewXDRState(XPT_DECODE, whole, flen);
        if (!XPT_MakeCursor(state, XPT_HEADER, 0, cursor)) {
            fprintf(stdout, "XPT_MakeCursor failed for %s\n", name);
            return 1;
        }
        if (!XPT_DoHeader(cursor, &header)) {
            fprintf(stdout,
                    "DoHeader failed for %s.  Is %s a valid .xpt file?\n",
                    name, name);
            return 1;
        }

        for (i = 0; i < header->num_interfaces; i++) {
            XPTInterfaceDirectoryEntry *ide = &header->interface_directory[i];
            char javaname[256];

            if (ide->interface_descriptor == NULL) {
                continue;
            }

            strcpy(javaname, ide->name);
            strcat(javaname, "__Proxy.java");

            out = fopen(javaname, "w");

            if (!out) {
                perror("FAILED: fopen");
                return 1;
            }

            if (!GenproxyClass(out, cursor, ide, header)) {
                return PR_FALSE;
                perror("FAILED: Cannot print interface");
                return 1;
            }

            fclose(out);
        }

        XPT_DestroyXDRState(state);
        free(whole);

    } else {
        fclose(in);
        perror("FAILED: file length <= 0");
        return 1;
    }

    return 0;
}

static const char *
classname_iid_define(const char *className)
{
    char *result = malloc(strlen(className) + 6);
    char *iidName;

    if (className[0] == 'n' && className[1] == 's') {
        /* backcompat naming styles */
        strcpy(result, "NS_");
        strcat(result, className + 2);
    } else {
        strcpy(result, className);
    }

    for (iidName = result; *iidName; iidName++) {
        *iidName = toupper(*iidName);
    }

    strcat(result, "_IID");
    return result;
}

PRBool
GenproxyClass(FILE *out,
              XPTCursor *cursor,
              XPTInterfaceDirectoryEntry *ide, 
              XPTHeader *header) {
    int i;
    XPTInterfaceDescriptor *id = ide->interface_descriptor;
    XPTInterfaceDirectoryEntry *parent_ide = NULL;
    const char *iidName = classname_iid_define(ide->name);  /* XXX: Leak */
    
    if (id->parent_interface) {
        parent_ide = &header->interface_directory[id->parent_interface - 1];
    }

    fprintf(out, "package %s;\n\n", 
            ide->name_space ? ide->name_space : "org.mozilla.xpcom");

    for (i = 0; i < header->num_interfaces; i++) {
        XPTInterfaceDirectoryEntry *impide = &header->interface_directory[i];
        if (impide != ide) {
            fprintf(out, "import %s.%s;\n", 
                    impide->name_space ? impide->name_space : "org.mozilla.xpcom",
                    impide->name);

        }
    }
    if (parent_ide) {
        fprintf(out, "import %s.%s__Proxy;\n", 
                parent_ide->name_space ? parent_ide->name_space : "org.mozilla.xpcom",
                parent_ide->name);
    }
    else {
        fprintf(out, "import org.mozilla.xpcom.XPComUtilities;\n");
    }
    fprintf(out, "import org.mozilla.xpcom.nsID;\n");

    fprintf(out, "\nclass %s__Proxy", ide->name);

    if (parent_ide) {
        fprintf(out, " extends %s__Proxy", parent_ide->name);
    }

    fprintf(out,  " implements %s {\n", ide->name);

    for (i = 0; i < id->num_methods; i++) {
        if (!GenproxyMethodPrototype(out, header, &id->method_descriptors[i])) {
            return PR_FALSE;
        }
        /* XXX: KLUDGE method index for now */
        if (!GenproxyMethodBody(out, header, 
                                &id->method_descriptors[i], i+3, iidName)) {
            return PR_FALSE;
        }
    }
    fprintf(out, "}\n");
    return PR_TRUE;
}

PRBool
GenproxyMethodPrototype(FILE *out, XPTHeader *header, XPTMethodDescriptor *md) {
    int i;
    char *param_type;
    int retval_ind = -1;
    XPTParamDescriptor *pd;

    if (XPT_MD_IS_HIDDEN(md->flags)) {
        return PR_TRUE;
    }

    /* Find index of retval */
    for (i = 0; i < md->num_args; i++) {
        pd = &md->params[i];
        if (XPT_PD_IS_RETVAL(pd->flags)) {
            retval_ind = i;
            break;
        }
    }

    if (XPT_MD_IS_GETTER(md->flags)) {
        pd = &md->params[0];

        if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
            return PR_FALSE;
        }
        fprintf(out, "    public %s get%c%s()", 
                param_type, 
                toupper(md->name[0]),
                (md->name)+1);
    }
    else if (XPT_MD_IS_SETTER(md->flags)) {
        pd = &md->params[0];

        if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
            return PR_FALSE;
        }
        fprintf(out, "    public void set%c%s(%s p0)", 
                toupper(md->name[0]),
                (md->name)+1,
                param_type);
    }
    else {
        if (retval_ind < 0) {
            param_type = "void";
        }
        else {
            pd = &md->params[retval_ind];

            if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
                return PR_FALSE;
            }
        }

        fprintf(out, "    public %s %s(", param_type, md->name);

        for (i = 0; i < md->num_args; i++) {
            pd = &md->params[i];

            if (XPT_PD_IS_RETVAL(pd->flags)) {
                continue;
            }

            if (i!=0) {
                fprintf(out, ", ");
            }
            if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
                return PR_FALSE;
            }
            fprintf(out, "%s", param_type);
            if (XPT_PD_IS_OUT(pd->flags)) {
                fprintf(out, "[]");
                /*
                  if (XPT_PD_IS_SHARED(pd->flags)) {
                  fprintf(out, "shared ");
                  }
                */
            }
            fprintf(out, " p%d", i);
        }
        fprintf(out, ")");
    }
    /*
     ??? XPT_MD_IS_VARARGS(md->flags)
     ??? XPT_MD_IS_CTOR(md->flags)
     */
    return PR_TRUE;
}

PRBool
GenproxyMethodBody(FILE *out, XPTHeader *header, 
                   XPTMethodDescriptor *md, int methodIndex, const char *iidName) {
    int i;
    char *param_type;
    char *ref_type;
    int retval_ind = -1;
    XPTParamDescriptor *pd;
    XPTTypeDescriptor *td;

    /* Find index of retval */
    for (i = 0; i < md->num_args; i++) {
        pd = &md->params[i];
        if (XPT_PD_IS_RETVAL(pd->flags)) {
            retval_ind = i;
            break;
        }
    }

    fprintf(out, " {\n");

    /* Now run through the param list yet again to fill in args */
    fprintf(out, "        Object[] args = new Object[%d];\n", md->num_args);
    for (i = 0; i < md->num_args; i++) {
        pd = &md->params[i];
        td = &pd->type;

        if (XPT_PD_IS_IN(pd->flags)) {
            if (XPT_TDP_IS_POINTER(td->prefix.flags)) {
                fprintf(out, "        args[%d] = p%d%s;\n", 
                        i, i, 
                        (XPT_PD_IS_OUT(pd->flags) ? "[0]" : ""));
            }
            else {
                if (!GenproxyGetStringForRefType(header, td, &ref_type)) {
                    return PR_FALSE;
                }
                fprintf(out, "        args[%d] = new %s(p%d%s);\n", 
                        i, ref_type, i, 
                        (XPT_PD_IS_OUT(pd->flags) ? "[0]" : ""));
            }
        }
        /*
          else {
          fprintf(out, "        args[%d] = null;\n", i);
          }
        */
    }
    fprintf(out, "        this.__invokeByIndex(%s, %d, args);\n",
            iidName, methodIndex);
    /* One more time with feeling to handle the out parameters */
    for (i = 0; i < md->num_args; i++) {
        pd = &md->params[i];
        td = &pd->type;

        if (XPT_PD_IS_RETVAL(pd->flags)) {
            continue;
        }

        if (XPT_PD_IS_OUT(pd->flags)) {
            if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
                return PR_FALSE;
            }

            if (XPT_TDP_IS_POINTER(td->prefix.flags)) {
                fprintf(out, "        p%d[0] = (%s)args[%d];\n", 
                        i, param_type, i);
            }
            else {
                if (!GenproxyGetStringForRefType(header, td, &ref_type)) {
                    return PR_FALSE;
                }
                fprintf(out, "        p%d[0] = ((%s)args[%d]).%sValue();\n", 
                        i, ref_type, i, param_type);
            }
        }
    }

    if (retval_ind >= 0) {
        pd = &md->params[retval_ind];
        td = &pd->type;

        if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
            return PR_FALSE;
        }

        if (XPT_TDP_IS_POINTER(td->prefix.flags)) {
            fprintf(out, "        return (%s)args[%d];\n", param_type, retval_ind);
        }
        else {
            if (!GenproxyGetStringForRefType(header, td, &ref_type)) {
                return PR_FALSE;
            }
            fprintf(out, "        return ((%s)args[%d]).%sValue();\n", 
                    ref_type, retval_ind, param_type);
        }
    }
    fprintf(out, "    }\n");
    /*
     ??? XPT_MD_IS_VARARGS(md->flags)
     ??? XPT_MD_IS_CTOR(md->flags)
     */
    return PR_TRUE;
}
    
PRBool
GenproxyGetStringForType(XPTHeader *header, XPTTypeDescriptor *td,
                     char **type_string)
{
    int tag = XPT_TDP_TAG(td->prefix);
    
    if (tag == TD_INTERFACE_TYPE) {
        int idx = td->type.interface;
        if (!idx || idx > header->num_interfaces)
            *type_string = "/*unknown*/ nsISupports";
        else
            *type_string = header->interface_directory[idx-1].name;
    } else if (XPT_TDP_IS_POINTER(td->prefix.flags)) {
        *type_string = rtype_array[tag];
    } else {
        *type_string = type_array[tag];
    }

    return PR_TRUE;
}

PRBool
GenproxyGetStringForRefType(XPTHeader *header, XPTTypeDescriptor *td,
                            char **type_string)
{
    int tag = XPT_TDP_TAG(td->prefix);
    
    if (tag == TD_INTERFACE_TYPE) {
        int idx = td->type.interface;
        if (!idx || idx > header->num_interfaces)
            *type_string = "nsISupports";
        else
            *type_string = header->interface_directory[idx-1].name;
    } else {
        *type_string = rtype_array[tag];
    }

    return PR_TRUE;
}

PRBool
GenproxyXPTString(FILE *out, XPTString *str)
{
    int i;
    for (i=0; i<str->length; i++) {
        fprintf(out, "%c", str->bytes[i]);
    }
    return PR_TRUE;
}

#if 0
PRBool
GenproxyHeader(XPTCursor *cursor, XPTHeader *header, 
               const int indent, PRBool verbose_mode) 
{
    int i;
    
    fprintf(stdout, "Header:\n");

    if (verbose_mode) {
        fprintf(stdout, "%*sMagic beans:           ", indent, " ");
        for (i=0; i<16; i++) {
            fprintf(stdout, "%02x", header->magic[i]);
        }
        fprintf(stdout, "\n");
        if (strncmp((const char*)header->magic, XPT_MAGIC, 16) == 0)
            fprintf(stdout, "%*s                       PASSED\n", indent, " ");
        else
            fprintf(stdout, "%*s                       FAILED\n", indent, " ");
    }
    fprintf(stdout, "%*sMajor version:         %d\n", indent, " ",
            header->major_version);
    fprintf(stdout, "%*sMinor version:         %d\n", indent, " ",
            header->minor_version);    
    fprintf(stdout, "%*sNumber of interfaces:  %d\n", indent, " ",
            header->num_interfaces);

    if (verbose_mode) {
        fprintf(stdout, "%*sFile length:           %d\n", indent, " ",
                header->file_length);
        fprintf(stdout, "%*sData pool offset:      %d\n\n", indent, " ",
                header->data_pool);
    }
    
    fprintf(stdout, "%*sAnnotations:\n", indent, " ");
    if (!GenproxyAnnotations(header->annotations, indent*2, verbose_mode))
        return PR_FALSE;

    fprintf(stdout, "\nInterface Directory:\n");
    for (i=0; i<header->num_interfaces; i++) {
        if (verbose_mode) {
            fprintf(stdout, "%*sInterface #%d:\n", indent, " ", i);
            if (!GenproxyInterfaceDirectoryEntry(cursor,
                                                 &header->interface_directory[i],
                                                 header, indent*2,
                                                 verbose_mode)) {
                return PR_FALSE;
            }
        } else {
            if (!GenproxyInterfaceDirectoryEntry(cursor,
                                                 &header->interface_directory[i],
                                                 header, indent,
                                                 verbose_mode)) {
                return PR_FALSE;
            }
        }
    }

    return PR_TRUE;
}    

PRBool
GenproxyAnnotations(XPTAnnotation *ann, const int indent, PRBool verbose_mode) 
{
    int i = -1;
    XPTAnnotation *last;
    int new_indent = indent + BASE_INDENT;

    do {
        i++;
        if (XPT_ANN_IS_PRIVATE(ann->flags)) {
            if (verbose_mode) {
                fprintf(stdout, "%*sAnnotation #%d is private.\n", 
                        indent, " ", i);
            } else {
                fprintf(stdout, "%*sAnnotation #%d:\n", 
                        indent, " ", i);
            }                
            fprintf(stdout, "%*sCreator:      ", new_indent, " ");
            if (!GenproxyXPTString(ann->creator))
                return PR_FALSE;
            fprintf(stdout, "\n");
            fprintf(stdout, "%*sPrivate Data: ", new_indent, " ");            
            if (!GenproxyXPTString(ann->private_data))
                return PR_FALSE;
            fprintf(stdout, "\n");
        } else {
            fprintf(stdout, "%*sAnnotation #%d is empty.\n", 
                    indent, " ", i);
        }
        last = ann;
        ann = ann->next;
    } while (!XPT_ANN_IS_LAST(last->flags));
        
    if (verbose_mode) {
        fprintf(stdout, "%*sAnnotation #%d is the last annotation.\n", 
                indent, " ", i);
    }

    return PR_TRUE;
}

static void
print_IID(struct nsID *iid, FILE *file)
{
    fprintf(file, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            (PRUint32) iid->m0, (PRUint32) iid->m1,(PRUint32) iid->m2,
            (PRUint32) iid->m3[0], (PRUint32) iid->m3[1],
            (PRUint32) iid->m3[2], (PRUint32) iid->m3[3],
            (PRUint32) iid->m3[4], (PRUint32) iid->m3[5],
            (PRUint32) iid->m3[6], (PRUint32) iid->m3[7]);
            
}

PRBool
GenproxyInterfaceDirectoryEntry(XPTCursor *cursor, 
                                XPTInterfaceDirectoryEntry *ide, 
                                XPTHeader *header, const int indent,
                                PRBool verbose_mode)
{
    int new_indent = indent + BASE_INDENT;

    if (verbose_mode) {
        fprintf(stdout, "%*sIID:                             ", indent, " ");
        print_IID(&ide->iid, stdout);
        fprintf(stdout, "\n");
        
        fprintf(stdout, "%*sName:                            %s\n", 
                indent, " ", ide->name);
        fprintf(stdout, "%*sNamespace:                       %s\n", 
                indent, " ", ide->name_space ? ide->name_space : "none");
        fprintf(stdout, "%*sAddress of interface descriptor: %p\n", 
                indent, " ", ide->interface_descriptor);

        fprintf(stdout, "%*sDescriptor:\n", indent, " ");
    
        if (!GenproxyInterfaceDescriptor(cursor, ide->interface_descriptor, 
                                         header, new_indent, verbose_mode)) {
            return PR_FALSE;
        }
    } else {
        fprintf(stdout, "%*s- %s::%s (", indent, " ", 
                ide->name_space ? ide->name_space : "", ide->name);
        print_IID(&ide->iid, stdout);
        fprintf(stdout, "):\n");
        if (!GenproxyInterfaceDescriptor(cursor, ide->interface_descriptor, 
                                         header, new_indent, verbose_mode)) {
            return PR_FALSE;
        }
    }

    return PR_TRUE;
}    

PRBool
GenproxyInterfaceDescriptor(XPTCursor *cursor, XPTInterfaceDescriptor *id,
                            XPTHeader *header, const int indent,
                            PRBool verbose_mode)
{
    XPTInterfaceDirectoryEntry *parent_ide;
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    if (!id) {
        fprintf(stdout, "%*s[Unresolved]\n", indent, " ");
        return PR_TRUE;
    }

    if (id->parent_interface) {

        parent_ide = &header->interface_directory[id->parent_interface - 1];

        fprintf(stdout, "%*sParent: %s::%s\n", indent, " ", 
                parent_ide->name_space ? 
                parent_ide->name_space : "", 
                parent_ide->name);
    }

    fprintf(stdout, "%*sFlags:\n", indent, " ");

    fprintf(stdout, "%*sScriptable: %s\n", new_indent, " ", 
            XPT_ID_IS_SCRIPTABLE(id->flags) ? "TRUE" : "FALSE");

    if (verbose_mode) {
        if (id->parent_interface) {
            fprintf(stdout, 
                    "%*sIndex of parent interface (in data pool): %d\n", 
                    indent, " ", id->parent_interface);
            
        }
    } else {
    }   

    if (id->num_methods > 0) {
        if (verbose_mode) {
            fprintf(stdout, 
                    "%*s# of Method Descriptors:                   %d\n", 
                    indent, " ", id->num_methods);
        } else {
            fprintf(stdout, "%*sMethods:\n", indent, " ");
        }

        for (i=0; i<id->num_methods; i++) {
            if (verbose_mode) {
                fprintf(stdout, "%*sMethod #%d:\n", new_indent, " ", i);
                if (!GenproxyMethodDescriptor(header,
                                              &id->method_descriptors[i], 
                                              more_indent, verbose_mode)) {
                    return PR_FALSE;
                } 
            } else { 
                if (!GenproxyMethodDescriptor(header,
                                              &id->method_descriptors[i], 
                                              new_indent, verbose_mode)) {
                    return PR_FALSE;
                } 
            }
        }        
    } else {
        fprintf(stdout, "%*sMethods:\n", indent, " ");
        fprintf(stdout, "%*sNo Methods\n", new_indent, " ");
    }
    
    if (id->num_constants > 0) {
        if (verbose_mode) {
            fprintf(stdout, 
                    "%*s# of Constant Descriptors:                  %d\n", 
                    indent, " ", id->num_constants);
        } else {
            fprintf(stdout, "%*sConstants:\n", indent, " ");
        }
        
        for (i=0; i<id->num_constants; i++) {
            if (verbose_mode) {
                fprintf(stdout, "%*sConstant #%d:\n", new_indent, " ", i);
                if (!GenproxyConstDescriptor(header, 
                                             &id->const_descriptors[i], 
                                             more_indent, verbose_mode))
                return PR_FALSE;
            } else {
                if (!GenproxyConstDescriptor(header, 
                                             &id->const_descriptors[i], 
                                             new_indent, verbose_mode)) {
                    return PR_FALSE;
                }
            }
        }
    } else { 
        fprintf(stdout, "%*sConstants:\n", indent, " ");
        fprintf(stdout, "%*sNo Constants\n", new_indent, " ");
    }

    return PR_TRUE;
}

PRBool
GenproxyMethodDescriptor(XPTHeader *header, XPTMethodDescriptor *md,
                         const int indent, PRBool verbose_mode)
{
    int i;
    int new_indent = indent + BASE_INDENT;
    int more_indent = new_indent + BASE_INDENT;

    if (verbose_mode) {
        fprintf(stdout, "%*sName:             %s\n", indent, " ", md->name);
        fprintf(stdout, "%*sIs Getter?        ", indent, " ");
        if (XPT_MD_IS_GETTER(md->flags))
            fprintf(stdout, "TRUE\n");
        else 
            fprintf(stdout, "FALSE\n");
        
        fprintf(stdout, "%*sIs Setter?        ", indent, " ");
        if (XPT_MD_IS_SETTER(md->flags))
            fprintf(stdout, "TRUE\n");
        else 
            fprintf(stdout, "FALSE\n");
        
        fprintf(stdout, "%*sIs Varargs?       ", indent, " ");
        if (XPT_MD_IS_VARARGS(md->flags))
            fprintf(stdout, "TRUE\n");
        else 
            fprintf(stdout, "FALSE\n");
        
        fprintf(stdout, "%*sIs Constructor?   ", indent, " ");
        if (XPT_MD_IS_CTOR(md->flags))
            fprintf(stdout, "TRUE\n");
        else 
            fprintf(stdout, "FALSE\n");
        
        fprintf(stdout, "%*sIs Hidden?        ", indent, " ");
        if (XPT_MD_IS_HIDDEN(md->flags))
            fprintf(stdout, "TRUE\n");
        else 
            fprintf(stdout, "FALSE\n");
        
        fprintf(stdout, "%*s# of arguments:   %d\n", indent, " ", md->num_args);
        fprintf(stdout, "%*sParameter Descriptors:\n", indent, " ");
        
        for (i=0; i<md->num_args; i++) {
            fprintf(stdout, "%*sParameter #%d:\n", new_indent, " ", i);
            
            if (!GenproxyParamDescriptor(header, &md->params[i], more_indent, 
                                         verbose_mode, PR_FALSE))
                return PR_FALSE;
        }
   
        fprintf(stdout, "%*sResult:\n", indent, " ");
        if (!GenproxyParamDescriptor(header, md->result, new_indent,
                                     verbose_mode, PR_TRUE)) {
            return PR_FALSE;
        }
    } else {
        char *param_type;
        XPTParamDescriptor *pd;

        if (!GenproxyGetStringForType(header, &md->result->type, &param_type)) {
            return PR_FALSE;
        }
        fprintf(stdout, "%*s%c%c%c%c%c %s %s(", indent - 6, " ",
                XPT_MD_IS_GETTER(md->flags) ? 'G' : ' ',
                XPT_MD_IS_SETTER(md->flags) ? 'S' : ' ',
                XPT_MD_IS_HIDDEN(md->flags) ? 'H' : ' ',
                XPT_MD_IS_VARARGS(md->flags) ? 'V' : ' ',
                XPT_MD_IS_CTOR(md->flags) ? 'C' : ' ',
                param_type, md->name);
        for (i=0; i<md->num_args; i++) {
            if (i!=0) {
                fprintf(stdout, ", ");
            }
            pd = &md->params[i];
            if (XPT_PD_IS_IN(pd->flags)) {
                fprintf(stdout, "in");
                if (XPT_PD_IS_OUT(pd->flags)) {
                    fprintf(stdout, "/out ");
                    if (XPT_PD_IS_RETVAL(pd->flags)) {
                        fprintf(stdout, "retval ");
                    }
                    if (XPT_PD_IS_SHARED(pd->flags)) {
                        fprintf(stdout, "shared ");
                    }
                } else {
                    fprintf(stdout, " ");
                }
            } else {
                if (XPT_PD_IS_OUT(pd->flags)) {
                    fprintf(stdout, "out ");
                    if (XPT_PD_IS_RETVAL(pd->flags)) {
                        fprintf(stdout, "retval ");
                    }
                    if (XPT_PD_IS_SHARED(pd->flags)) {
                        fprintf(stdout, "shared ");
                    }
                } else {
                    param_problems = PR_TRUE;
                    fprintf(stdout, "XXX ");
                }
            }
            if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
                return PR_FALSE;
            }
            fprintf(stdout, "%s", param_type);
        }
        fprintf(stdout, ");\n");   
    }
    return PR_TRUE;
}
    
PRBool
GenproxyParamDescriptor(XPTHeader *header, XPTParamDescriptor *pd,
                        const int indent, PRBool verbose_mode, 
                        PRBool is_result)
{
    int new_indent = indent + BASE_INDENT;
    
    if (!XPT_PD_IS_IN(pd->flags) && 
        !XPT_PD_IS_OUT(pd->flags) &&
        (XPT_PD_IS_RETVAL(pd->flags) ||
         XPT_PD_IS_SHARED(pd->flags))) {
        param_problems = PR_TRUE;
        fprintf(stdout, "XXX\n");
    } else {
        if (!XPT_PD_IS_IN(pd->flags) && !XPT_PD_IS_OUT(pd->flags)) {
            if (is_result) {
                if (XPT_TDP_TAG(pd->type.prefix) != TD_UINT32 &&
                    XPT_TDP_TAG(pd->type.prefix) != TD_VOID) {
                    param_problems = PR_TRUE;
                    fprintf(stdout, "XXX\n");   
                }
            } else {
                param_problems = PR_TRUE;
                fprintf(stdout, "XXX\n");
            }
        }
    }

    fprintf(stdout, "%*sIn Param?   ", indent, " ");
    if (XPT_PD_IS_IN(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");
    
    fprintf(stdout, "%*sOut Param?  ", indent, " ");
    if (XPT_PD_IS_OUT(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");
    
    fprintf(stdout, "%*sRetval?     ", indent, " ");
    if (XPT_PD_IS_RETVAL(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sShared?     ", indent, " ");
    if (XPT_PD_IS_SHARED(pd->flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sType Descriptor:\n", indent, " ");
    if (!GenproxyTypeDescriptor(&pd->type, new_indent, verbose_mode))
        return PR_FALSE;
    
    return PR_TRUE;
}

PRBool
GenproxyTypeDescriptor(XPTTypeDescriptor *td, int indent, PRBool verbose_mode)
{
    int new_indent = indent + BASE_INDENT;

    fprintf(stdout, "%*sIs Pointer?        ", indent, " ");
    if (XPT_TDP_IS_POINTER(td->prefix.flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Unique Pointer? ", indent, " ");
    if (XPT_TDP_IS_UNIQUE_POINTER(td->prefix.flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sIs Reference?      ", indent, " ");
    if (XPT_TDP_IS_REFERENCE(td->prefix.flags))
        fprintf(stdout, "TRUE\n");
    else 
        fprintf(stdout, "FALSE\n");

    fprintf(stdout, "%*sTag:               %d\n", indent, " ", 
            XPT_TDP_TAG(td->prefix));
    
    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
        fprintf(stdout, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stdout, "%*sIndex of IDE:             %d\n", new_indent, " ", 
                td->type.interface);
    }

    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_IS_TYPE) {
        fprintf(stdout, "%*sInterfaceTypeDescriptor:\n", indent, " "); 
        fprintf(stdout, "%*sIndex of Method Argument: %d\n", new_indent, " ", 
                td->type.argnum);        
    }

    return PR_TRUE;
}

PRBool
GenproxyConstDescriptor(XPTHeader *header, XPTConstDescriptor *cd,
                        const int indent, PRBool verbose_mode)
{
    int new_indent = indent + BASE_INDENT;
    char *const_type;
/*      char *out; */
    PRUint32 uintout;
    PRInt32 intout;

    if (verbose_mode) {
        fprintf(stdout, "%*sName:   %s\n", indent, " ", cd->name);
        fprintf(stdout, "%*sType Descriptor: \n", indent, " ");
        if (!GenproxyTypeDescriptor(&cd->type, new_indent, verbose_mode))
            return PR_FALSE;
        fprintf(stdout, "%*sValue:  ", indent, " ");
    } else {
        if (!GenproxyGetStringForType(header, &cd->type, &const_type)) {
            return PR_FALSE;
        }
        fprintf(stdout, "%*s%s %s = ", indent, " ", const_type, cd->name);
    }        

    switch(XPT_TDP_TAG(cd->type.prefix)) {
    case TD_INT8:
        fprintf(stdout, "%d", cd->value.i8);
        break;
    case TD_INT16:
        fprintf(stdout, "%d", cd->value.i16);
        break;
    case TD_INT64:
        /* XXX punt for now to remove NSPR linkage...
         * borrow from mozilla/nsprpub/pr/src/io/prprf.c::cvt_ll? */

/*          out = PR_smprintf("%lld", cd->value.i64); */
/*          fputs(out, stdout); */
/*          PR_smprintf_free(out); */
        LL_L2I(intout, cd->value.i64);
        fprintf(stdout, "%d", intout);
        break;
    case TD_INT32:
        fprintf(stdout, "%d", cd->value.i32);
        break;
    case TD_UINT8:
        fprintf(stdout, "%d", cd->value.ui8);
        break;
    case TD_UINT16:
        fprintf(stdout, "%d", cd->value.ui16);
        break;
    case TD_UINT64:
/*          out = PR_smprintf("%lld", cd->value.ui64); */
/*          fputs(out, stdout); */
/*          PR_smprintf_free(out); */
        /* XXX punt for now to remove NSPR linkage. */
        LL_L2UI(uintout, cd->value.ui64);
        fprintf(stdout, "%d", uintout);
        break;
    case TD_UINT32:
        fprintf(stdout, "%d", cd->value.ui32);
        break;
    case TD_FLOAT:
        fprintf(stdout, "%f", cd->value.flt);
        break;
    case TD_DOUBLE:
        fprintf(stdout, "%g", cd->value.dbl);
        break;
    case TD_BOOL:
        if (cd->value.bul)
            fprintf(stdout, "TRUE");
        else 
            fprintf(stdout, "FALSE");
            break;
    case TD_CHAR:
        fprintf(stdout, "%c", cd->value.ch);
        break;
    case TD_WCHAR:
        fprintf(stdout, "%c", cd->value.wch & 0xff);
        break;
    case TD_VOID:
        fprintf(stdout, "VOID");
        break;
    case TD_PNSIID:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            print_IID(cd->value.iid, stdout);
        } else 
            return PR_FALSE;
        break;
    case TD_PBSTR:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            if (!GenproxyXPTString(cd->value.string))
                return PR_FALSE;
        } else 
            return PR_FALSE;
        break;            
    case TD_PSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            fprintf(stdout, "%s", cd->value.str);
        } else 
            return PR_FALSE;
        break;
    case TD_PWSTRING:
        if (XPT_TDP_IS_POINTER(cd->type.prefix.flags)) {
            PRUint16 *ch = cd->value.wstr;
            while (*ch) {
                fprintf(stdout, "%c", *ch & 0xff);
                ch++;
            }
        } else 
            return PR_FALSE;
        break;
    default:
        perror("Unrecognized type");
        return PR_FALSE;
        break;   
    }
    
    if (verbose_mode) {
        fprintf(stdout, "\n");
    } else {
        fprintf(stdout, ";\n");
    }

    return PR_TRUE;
}

#endif
