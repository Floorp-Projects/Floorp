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
              XPTHeader *header,
              PRBool iface);

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
    fprintf(stdout, "Usage: %s [-v] [-i] <filename.xpt>\n"
            "       -v verbose mode\n", argv[0]);
}

#ifdef XP_MAC

static int mac_get_args(char*** argv)
{
	static char* args[] = { "genproxy", NULL, NULL };
	static StandardFileReply reply;
	
	*argv = args;
	
	printf("choose an .xpt file to parse.\n");
	
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
    PRBool interface_mode = PR_FALSE;
    PRBool bytecode_mode = PR_FALSE;
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    size_t flen;
    char *name;
    const char *dirname = NULL;
    char *whole;
    FILE *in;
    FILE *out;
    int i;
    int c;

#ifdef XP_MAC
    if (argc == 0 || argv == NULL)
	argc = mac_get_args(&argv);
#endif

    while ((c = getopt(argc, argv, "vibd:")) != EOF) {
        switch (c) {
        case 'v':
            verbose_mode = PR_TRUE;
            break;
        case 'i':
            interface_mode = PR_TRUE;
            break;
        case 'b':
            bytecode_mode = PR_TRUE;
            break;
        case 'd':
            dirname = optarg;
            break;
        case '?':
            genproxy_usage(argv);
            return 1;
        }
    }

    name = argv[optind];
    flen = get_file_length(name);
    in = fopen(name, "rb");

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

            javaname[0] = 0;

            if (ide->interface_descriptor == NULL) {
                continue;
            }

            /* XXX: is this XP? */
            if (dirname != NULL) {
                int len;
                strcpy(javaname, dirname);
                len = strlen(javaname);
                if (javaname[len - 1] != '/') {
                    javaname[len] = '/';
                    javaname[len+1] = 0;
                }
            }
            strcat(javaname, ide->name);
            if (interface_mode) {
                strcat(javaname, ".java");
            }
            else {
                strcat(javaname, "__Proxy.java");
            }

            out = fopen(javaname, "w");

            if (!out) {
                perror("FAILED: fopen");
                return 1;
            }

            if (!GenproxyClass(out, cursor, ide, header, interface_mode)) {
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
GenproxyClass(FILE *out,
              XPTCursor *cursor,
              XPTInterfaceDirectoryEntry *ide, 
              XPTHeader *header,
              PRBool iface) {
    int i;
    XPTInterfaceDescriptor *id = ide->interface_descriptor;
    XPTInterfaceDirectoryEntry *parent_ide = NULL;
    const char *iidName = classname_iid_define(ide->name);
    
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
    else if (!iface) {
        fprintf(out, "import org.mozilla.xpcom.XPComUtilities;\n");
        fprintf(out, "import org.mozilla.xpcom.ComObject;\n");
    }
    fprintf(out, "import org.mozilla.xpcom.nsID;\n");

    if (iface) {
        fprintf(out, "\npublic interface %s", ide->name);

        if (parent_ide) {
            fprintf(out, " extends %s", parent_ide->name);
        }
        fprintf(out,  " {\n", ide->name);

        fprintf(out, 
                "\n    public static final String %s_STRING = \"", iidName);

        print_IID(&ide->iid, out);

        fprintf(out, "\";\n\n");

        fprintf(out, 
                "    public static final nsID %s = new nsID(%s_STRING);\n", 
                iidName, iidName);
    }
    else {
        fprintf(out, "\nclass %s__Proxy", ide->name);

        if (parent_ide) {
            fprintf(out, " extends %s__Proxy", parent_ide->name);
        }
        fprintf(out,  " implements %s {\n", ide->name);
    }

    for (i = 0; i < id->num_methods; i++) {
        XPTMethodDescriptor *md = &id->method_descriptors[i];

        if (XPT_MD_IS_HIDDEN(md->flags)) {
            continue;
        }

        if (!GenproxyMethodPrototype(out, header, md)) {
            return PR_FALSE;
        }
        if (iface) {
            fprintf(out, ";\n");
        }
        else {
            /* XXX: KLUDGE method index for now */
            if (!GenproxyMethodBody(out, header, md, i+3, iidName)) {
                return PR_FALSE;
            }
        }
        fprintf(out, "\n");
    }
    fprintf(out, "}\n");
    free((void *)iidName);
    return PR_TRUE;
}

PRBool
GenproxyMethodPrototype(FILE *out, XPTHeader *header, XPTMethodDescriptor *md) {
    int i;
    char *param_type;
    int retval_ind = -1;
    XPTParamDescriptor *pd;

    /* Find index of retval */
    for (i = 0; i < md->num_args; i++) {
        pd = &md->params[i];
        if (XPT_PD_IS_RETVAL(pd->flags)) {
            retval_ind = i;
            break;
        }
    }

    /* "Hidden" methods are protected (if reflected at all) */
    if (XPT_MD_IS_HIDDEN(md->flags)) {
        fprintf(out, "    protected ");
    }
    else {
        fprintf(out, "    public ");
    }

    if (XPT_MD_IS_GETTER(md->flags)) {
        pd = &md->params[0];

        if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
            return PR_FALSE;
        }
        fprintf(out, "%s get%c%s()", 
                param_type, 
                toupper(md->name[0]),
                (md->name)+1);
    }
    else if (XPT_MD_IS_SETTER(md->flags)) {
        pd = &md->params[0];

        if (!GenproxyGetStringForType(header, &pd->type, &param_type)) {
            return PR_FALSE;
        }
        fprintf(out, "void set%c%s(%s p0)", 
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

        fprintf(out, "%s %s(", param_type, md->name);

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


