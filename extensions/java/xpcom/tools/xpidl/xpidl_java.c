/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Allen (michael.allen@sun.com)
 *   Frank Mitchell (frank.mitchell@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Generate Java interfaces from XPIDL.
 */

#include "xpidl.h"
#include <ctype.h>
#include <glib.h>

#ifdef XP_WIN
#include "windef.h"
#define PATH_MAX  MAX_PATH
#endif

static char* subscriptIdentifier(TreeState *state, char *str);

/*
struct java_priv_data {
    GHashTable *typedefTable;
};
*/

static char* javaKeywords[] = {
    "abstract", "default", "if"        , "private"     , "this"     ,
    "boolean" , "do"     , "implements", "protected"   , "throw"    ,
    "break"   , "double" , "import",     "public"      , "throws"   ,
    "byte"    , "else"   , "instanceof", "return"      , "transient",
    "case"    , "extends", "int"       , "short"       , "try"      ,
    "catch"   , "final"  , "interface" , "static"      , "void"     ,
    "char"    , "finally", "long"      , "strictfp"    , "volatile" ,
    "class"   , "float"  , "native"    , "super"       , "while"    ,
    "const"   , "for"    , "new"       , "switch"      , "assert"   ,
    "continue", "goto"   , "package"   , "synchronized",
    "clone"   ,  /* clone is a member function of java.lang.Object */
    "finalize"   /* finalize is a member function of java.lang.Object */};

#define TYPEDEFS(state)     (((struct java_priv_data *)state->priv)->typedefTable)
#define PRIVDATA(state)     (((struct java_priv_data *)state->priv))
#define KEYWORDS(state)     (((struct java_priv_data *)state->priv)->keywords)

static gboolean
write_classname_iid_define(FILE *file, const char *className)
{
    const char *iidName;
    if (className[0] == 'n' && className[1] == 's') {
        /* backcompat naming styles */
        fputs("NS_", file);
        iidName = className + 2;
    } else {
        iidName = className;
    }

    while (*iidName) {
        fputc(toupper(*iidName++), file);
    }

    fputs("_IID", file);
    return TRUE;
}

static gboolean
java_prolog(TreeState *state)
{
    int len, i;
    state->priv = calloc(1, sizeof(struct java_priv_data));
    if (!state->priv)
        return FALSE;

    TYPEDEFS(state) = 0;
    TYPEDEFS(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!TYPEDEFS(state)) {
        /* XXX report error */
        free(state->priv);
        return FALSE;
    }

    KEYWORDS(state) = 0;
    KEYWORDS(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!KEYWORDS(state)) {
        g_hash_table_destroy(TYPEDEFS(state));
        free(state->priv);
        return FALSE;
    }
    len = sizeof(javaKeywords)/sizeof(*javaKeywords);
    for (i = 0; i < len; i++) {
        g_hash_table_insert(KEYWORDS(state),
                            javaKeywords[i],
                            javaKeywords[i]);
    }

    return TRUE;
}

static gboolean 
java_epilog(TreeState *state)
{
    /* points to other elements of the tree, so just destroy the table */
    g_hash_table_destroy(TYPEDEFS(state));
    g_hash_table_destroy(KEYWORDS(state));

    free(state->priv);
    state->priv = NULL;
    
    return TRUE;
}

static gboolean
forward_declaration(TreeState *state) 
{
    /*
     * Java doesn't need forward declarations unless the declared 
     * class resides in a different package.
     */
#if 0
    IDL_tree iface = state->tree;
    const char *className = IDL_IDENT(IDL_FORWARD_DCL(iface).ident).str;
    const char *pkgName = "org.mozilla.xpcom";
    if (!className)
        return FALSE;
    /* XXX: Get package name and compare */
    fprintf(state->file, "import %s.%s;\n", pkgName, className);
#endif

    return TRUE;
}


static gboolean
interface_declaration(TreeState *state) 
{
    char outname[PATH_MAX];
    char* p;
    IDL_tree interface = state->tree;
    IDL_tree iterator = NULL;
    char *interface_name = 
        subscriptIdentifier(state, IDL_IDENT(IDL_INTERFACE(interface).ident).str);
    const char *iid = NULL;
    char iid_parsed[UUID_LENGTH];

    if (!verify_interface_declaration(interface))
        return FALSE;

    /*
     * Each interface decl is a single file
     */
    p = strrchr(state->filename, '/');
    if (p) {
      strncpy(outname, state->filename, p + 1 - state->filename);
      outname[p + 1 - state->filename] = '\0';
    }
    strcat(outname, interface_name);
    strcat(outname, ".java");

    state->file =  fopen(outname, "w");
    if (!state->file) {
        perror("error opening output file");
        return FALSE;
    }

    fputs("/*\n * ************* DO NOT EDIT THIS FILE ***********\n",
          state->file);
    
    fprintf(state->file, 
            " *\n * This file was automatically generated from %s.idl.\n", 
            state->basename);
    
    fputs(" */\n\n", state->file);

    if (state->package)
        fprintf(state->file, "\npackage %s;\n\n", state->package);

    if (!state->package || strcmp(state->package, "org.mozilla.xpcom") != 0)
        fputs("import org.mozilla.xpcom.*;\n", state->file);

    fputs("\n", state->file);

#ifndef LIBIDL_MAJOR_VERSION
    iid = IDL_tree_property_get(interface, "uuid");
#else
    iid = IDL_tree_property_get(IDL_INTERFACE(interface).ident, "uuid");
#endif
    if (iid) {
        struct nsID id;
        /* Redundant, but a better error than 'cannot parse.' */
        if (strlen(iid) != 36) {
            IDL_tree_error(state->tree, "IID %s is the wrong length\n", iid);
            return FALSE;
        }

        /*
         * Parse uuid and then output resulting nsID to string, to validate
         * uuid and normalize resulting .h files.
         */
        if (!xpidl_parse_iid(&id, iid)) {
            IDL_tree_error(state->tree, "cannot parse IID %s\n", iid);
            return FALSE;
        }
        if (!xpidl_sprint_iid(&id, iid_parsed)) {
            IDL_tree_error(state->tree, "error formatting IID %s\n", iid);
            return FALSE;
        }
    } else {
        IDL_tree_error(state->tree, "interface %s lacks a uuid attribute\n", 
                       interface_name);
        return FALSE;
    }

    /*
     * Write out JavaDoc comment
     */

    fprintf(state->file, "\n/**\n * Interface %s\n", interface_name);
    if (iid != NULL) {
        fprintf(state->file, " *\n * IID: 0x%s\n */\n\n", iid_parsed);
    } else {
        fputs(" */\n\n", state->file);
    }

    /*
     * Write "public interface <foo>"
     */

    fprintf(state->file, "public interface %s", interface_name);

    /*
     * Check for inheritence, and iterator over the inherited names,
     * if any.
     */

    if ((iterator = IDL_INTERFACE(interface).inheritance_spec)) {
        fputs(" extends ", state->file);

        do {
            fprintf(state->file, "%s", 
                    IDL_IDENT(IDL_LIST(iterator).data).str);
	    
            if (IDL_LIST(iterator).next) {
                fputs(", ", state->file);
            }
        } while ((iterator = IDL_LIST(iterator).next));
    }

    fputs("\n{\n", state->file);
    
    /*
     * Write interface constants for IID
     */
    if (iid) {
        /* public static final String NS_ISUPPORTS_IID = "00000000-0000-0000-c000-000000000046" */
        fputs("    public static final String ", state->file);
        write_classname_iid_define(state->file, interface_name);
        fprintf(state->file, " =\n        \"{%s}\";\n\n", iid_parsed);
    }

    /*
     * Advance the state of the tree, go on to process more
     */
    
    state->tree = IDL_INTERFACE(interface).body;

	PRIVDATA(state)->bCountingMethods = FALSE;
	PRIVDATA(state)->numMethods = 0;

    if (state->tree && !xpidl_process_node(state)) {
        return FALSE;
    }


    fputs("\n}\n", state->file);

    fprintf(state->file, "\n/*\n * end\n */\n");
    fclose(state->file);

    return TRUE;
}

static gboolean
process_list(TreeState *state)
{
    IDL_tree iter;
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        if (!xpidl_process_node(state))
            return FALSE;
    }
    return TRUE;
}

static gboolean 
xpcom_to_java_type(TreeState *state, IDL_tree type) 
{
    IDL_tree real_type;
    IDL_tree up;

    if (!type) {
        fputs("Object", state->file);
        return TRUE;
    }

    /* Could be a typedef; try to map it to the real type */
    real_type = find_underlying_type(type);
    type = real_type ? real_type : type;

    switch(IDL_NODE_TYPE(type)) {

    case IDLN_TYPE_INTEGER: {

        switch(IDL_TYPE_INTEGER(type).f_type) {

        case IDL_INTEGER_TYPE_SHORT:
            fputs("short", state->file);
            break;

        case IDL_INTEGER_TYPE_LONG:
            fputs("int", state->file);
            break;

        case IDL_INTEGER_TYPE_LONGLONG:
            fputs("long", state->file);
            break;
	    
        default:
            g_error("   Unknown integer type: %d\n",
                    IDL_TYPE_INTEGER(type).f_type);
            return FALSE;

        }

        break;
    }

    case IDLN_TYPE_CHAR:
    case IDLN_TYPE_WIDE_CHAR:
        fputs("char", state->file);
        break;

    case IDLN_TYPE_WIDE_STRING:
    case IDLN_TYPE_STRING:
        fputs("String", state->file);
        break;

    case IDLN_TYPE_BOOLEAN:
        fputs("boolean", state->file);
        break;

    case IDLN_TYPE_OCTET:
        fputs("byte", state->file);
        break;

    case IDLN_TYPE_FLOAT:
        switch(IDL_TYPE_FLOAT(type).f_type) {

        case IDL_FLOAT_TYPE_FLOAT:
            fputs("float", state->file);
            break;

        case IDL_FLOAT_TYPE_DOUBLE:
            fputs("double", state->file);
            break;
	    
        default:
            g_error("    Unknown floating point typ: %d\n",
                    IDL_NODE_TYPE(type));
            break;
        }
        break;


    case IDLN_IDENT:
      if (!(up = IDL_NODE_UP(type))) {
          IDL_tree_error(state->tree,
                         "ERROR: orphan ident %s in param list\n",
                         IDL_IDENT(state->tree).str);
          return FALSE;
      }
      switch (IDL_NODE_TYPE(up)) {
        case IDLN_FORWARD_DCL:
        case IDLN_INTERFACE: {
          char *className;
          const char *iid_is;
handle_iid_is:
          /* might get here via the goto, so re-check type */
          if (IDL_NODE_TYPE(up) == IDLN_INTERFACE)
              className = IDL_IDENT(IDL_INTERFACE(up).ident).str;
          else if (IDL_NODE_TYPE(up) == IDLN_FORWARD_DCL)
              className = IDL_IDENT(IDL_FORWARD_DCL(up).ident).str;
          else
              className = IDL_IDENT(IDL_NATIVE(up).ident).str;
          iid_is = NULL;

          if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL) {
              iid_is =
                  IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                        "iid_is");
          }
          if (iid_is) {
              fputs("nsISupports", state->file);
          } else {
              /* XXX How do we want to handle this? If it's an IDLN_INTERFACE,
               *  then we can just output the name of the class, since the IDL
               *  files exist for those classes.  However, if it's an
               *  IDLN_FORWARD_DCL, some of those interfaces are not defined in
               *  IDL files, so we get an error when trying to compile the java
               *  files.  So, for now, we just output them as the base iface
               *  (nsISupports). 
               */
              if (IDL_NODE_TYPE(up) == IDLN_FORWARD_DCL)
                  fputs("nsISupports", state->file);
              else
                  fprintf(state->file, "%s", className);
          }
          break;
        }
        case IDLN_NATIVE: {
            char *ident;

            /* jband - adding goto for iid_is when type is native */
            if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL &&
                IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                        "iid_is"))
            {
                goto handle_iid_is;
/*                fputs("nsISupports", state->file); */
                break;
            }

            ident = IDL_IDENT(type).str;
            if (IDL_tree_property_get(type, "nsid")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "domstring")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "astring")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "utf8string")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "cstring")) {
                fputs("String", state->file);
            } else {
                const char* user_type = IDL_NATIVE(IDL_NODE_UP(type)).user_type;
                IDL_tree real_type = 
                    g_hash_table_lookup(TYPEDEFS(state), user_type);

                if (real_type) {
                    return xpcom_to_java_type(state, real_type);
                } else {
                    if (strcmp(user_type, "PRInt8") == 0 ||
                        strcmp(user_type, "PRUint8") == 0) {
                        fputs("byte", state->file);
                    } else if (strcmp(user_type, "PRInt16") == 0 ||
                               strcmp(user_type, "PRUint16") == 0) {
                        fputs("short", state->file);
                    } else if (strcmp(user_type, "PRInt32") == 0 ||
                               strcmp(user_type, "PRUint32") == 0 ||
                               strcmp(user_type, "int") == 0) {
                        fputs("int", state->file);
                    } else if (strcmp(user_type, "PRInt64") == 0 ||
                               strcmp(user_type, "PRUint64") == 0) {
                        fputs("long", state->file);
                    } else if (strcmp(user_type, "PRBool") == 0) {
                        fputs("boolean", state->file);
                    } else if (strncmp(user_type, "char", 4) == 0 ||
                               strncmp(user_type, "const char", 10) == 0 ||
                               strncmp(user_type, "unsigned char", 13) == 0) {
                        if (IDL_tree_property_get(type, "ptr")) {
                            fputs("byte[]", state->file);
                        } else {
                            fputs("char", state->file);
                        }
                    } else if (strcmp(user_type, "nsIID") == 0) {
                        fputs("String", state->file);
                    } else if (strcmp(user_type, "nsString") == 0 ||
                               strcmp(user_type, "nsAString") == 0 ||
                               strcmp(user_type, "nsACString") == 0) {
                        fputs("String", state->file);
                    } else {
                        fputs("int", state->file);
                    }
                }
            }
            break;
          }
        default:
          if (IDL_NODE_TYPE(IDL_NODE_UP(up)) == IDLN_TYPE_DCL) {
              /* restart with the underlying type */
              IDL_tree new_type;
              new_type = IDL_TYPE_DCL(IDL_NODE_UP(up)).type_spec;
              if (new_type) {
                  gboolean rc = xpcom_to_java_type(state, new_type);
                  return rc;
              } else {
                  /* do what we would do in recursion if !type */
                  fputs("Object", state->file);
                  return TRUE;
              }
          }
          IDL_tree_error(state->tree,
                         "can't handle %s ident in param list\n",
                         "that type of"
                         );
          return FALSE;
      }
      break;

    default:
      IDL_tree_error(state->tree, "can't handle %s in param list\n",
#ifdef DEBUG_shaver
                     /* XXX is this safe to use on Win now? */
                     IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
#else
                  "that type"
#endif
      );
      return FALSE;
    }

    return TRUE;

}

static gboolean
xpcom_to_java_param(TreeState *state) 
{
    IDL_tree param = state->tree;

    /*
     * Put in type of parameter
     */

    if (!xpcom_to_java_type(state, IDL_PARAM_DCL(param).param_type_spec)) {
        return FALSE;
    }

    /*
     * If the parameter is out or inout, make it a Java array of the
     * appropriate type
     */

    if (IDL_PARAM_DCL(param).attr != IDL_PARAM_IN) {
        fputs("[]", state->file);
    }

    /*
     * If the parameter is an array make it a Java array
     */
    if (IDL_tree_property_get(IDL_PARAM_DCL(param).simple_declarator, "array"))
        fputs("[]", state->file);

    /*
     * Put in name of parameter 
     */

    fputc(' ', state->file);

    fputs(subscriptIdentifier(state, 
                              IDL_IDENT(IDL_PARAM_DCL(param).simple_declarator).str), 
          state->file);

    return TRUE;
}


static gboolean
type_declaration(TreeState *state) 
{
    /*
     * Unlike C, Java has no type declaration directive.
     * Instead, we record the mapping, and look up the actual type
     * when needed.
     */
    IDL_tree type = IDL_TYPE_DCL(state->tree).type_spec;
    IDL_tree dcls = IDL_TYPE_DCL(state->tree).dcls;

    /* XXX: check for illegal types */

    g_hash_table_insert(TYPEDEFS(state),
                        IDL_IDENT(IDL_LIST(dcls).data).str,
                        type);

    return TRUE;
}

static gboolean
method_declaration(TreeState *state) 
{
    const char* array = NULL;
    struct _IDL_OP_DCL *method = &IDL_OP_DCL(state->tree);
    gboolean method_notxpcom = 
        (IDL_tree_property_get(method->ident, "notxpcom") != NULL);
    gboolean method_noscript = 
        (IDL_tree_property_get(method->ident, "noscript") != NULL);
    IDL_tree iterator = NULL;
    IDL_tree retval_param = NULL;
    char *method_name =
                  g_strdup_printf("%c%s",
                                  tolower(IDL_IDENT(method->ident).str[0]),
                                  IDL_IDENT(method->ident).str + 1);

    if (!verify_method_declaration(state->tree))
        return FALSE;

    fputc('\n', state->file);
    xpidl_write_comment(state, 4);

#if 1
    /* do not write non-xpcom methods */
    if (method_notxpcom) {
        return TRUE;
    }

    /*
     * Write beginning of method declaration
     */
    fputs("    public ", state->file);
#else
    /* do not write nonscriptable methods */
    if (method_notxpcom || method_noscript) {
        return TRUE;
    }

    /*
     * Write beginning of method declaration
     */
    fputs("    ", state->file);
    if (!method_noscript) {
        /* Nonscriptable methods become package-protected */
        fputs("public ", state->file);
    }
#endif

    /*
     * Write return type
     * Unlike C++ headers, Java interfaces return the declared 
     * return value; an exception indicates XPCOM method failure.
     */
    if (method->op_type_spec) {
        state->tree = method->op_type_spec;
        if (!xpcom_to_java_type(state, method->op_type_spec)) {
            return FALSE;
        }
    } else {
        /* Check for retval attribute */
        for (iterator = method->parameter_dcls; iterator != NULL; 
             iterator = IDL_LIST(iterator).next) {

            IDL_tree original_tree = state->tree;

            state->tree = IDL_LIST(iterator).data;

            if (IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator, 
                                      "retval")) {
                retval_param = iterator;

                array = 
                    IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                          "array");

                /*
                 * Put in type of parameter
                 */
                if (!xpcom_to_java_type(state,
                                        IDL_PARAM_DCL(state->tree).param_type_spec)) {
                    return FALSE;
                }
                if (array)
                    fputs("[]", state->file);
            }

            state->tree = original_tree;
        }

        if (retval_param == NULL) {
            fputs("void", state->file);
        }
    }
 
    /*
     * Write method name
     */
    fprintf(state->file, " %s(", subscriptIdentifier(state, method_name));

    /*
     * Write parameters
     */
    for (iterator = method->parameter_dcls; iterator != NULL; 
         iterator = IDL_LIST(iterator).next) {

        /* Skip "retval" */
        if (iterator == retval_param) {
            continue;
        }

        if (iterator != method->parameter_dcls) {
            fputs(", ", state->file);
        }
        
        state->tree = IDL_LIST(iterator).data;

        if (!xpcom_to_java_param(state)) {
            return FALSE;
        }
    }

    fputs(")", state->file);

    /* XXX
       Disable this for now.  How do we specify exceptions?
    if (method->raises_expr) {
        IDL_tree iter = method->raises_expr;
        IDL_tree dataNode = IDL_LIST(iter).data;

        fputs(" throws ", state->file);
        fputs(IDL_IDENT(dataNode).str, state->file);
        iter = IDL_LIST(iter).next;

        while (iter) {
            dataNode = IDL_LIST(iter).data;
            fprintf(state->file, ", %s", IDL_IDENT(dataNode).str);
            iter = IDL_LIST(iter).next;
        }
    }   */

    fputs(";\n", state->file);

    return TRUE;
    
}


static gboolean
constant_declaration(TreeState *state)
{
    /*
     * The C++ header XPIDL module only allows for shorts and longs (ints)
     * to be constants, so we will follow the same convention
     */

    struct _IDL_CONST_DCL *declaration = &IDL_CONST_DCL(state->tree);
    const char *name = IDL_IDENT(declaration->ident).str;
    IDL_tree real_type;
    
    gboolean success;
    gboolean isshort = FALSE;

    if (!verify_const_declaration(state->tree))
        return FALSE;

    /* Could be a typedef; try to map it to the real type. */
    real_type = find_underlying_type(declaration->const_type);
    real_type = real_type ? real_type : declaration->const_type;

    /*
     * Consts must be in an interface
     */

    if (!IDL_NODE_UP(IDL_NODE_UP(state->tree)) ||
        IDL_NODE_TYPE(IDL_NODE_UP(IDL_NODE_UP(state->tree))) != 
        IDLN_INTERFACE) {

        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "A constant \"%s\" was declared outside an interface."
                       "  It was ignored.", name));

        return TRUE;
    }

    /*
     * Make sure this is a numeric short or long constant.
     */

    success = (IDLN_TYPE_INTEGER == IDL_NODE_TYPE(real_type));

    if (success) {
        /*
         * We aren't successful yet, we know it's an integer, but what *kind*
         * of integer?
         */

        switch(IDL_TYPE_INTEGER(real_type).f_type) {

        case IDL_INTEGER_TYPE_SHORT:
            /*
             * We're OK
             */
            isshort = TRUE;
            break;

        case IDL_INTEGER_TYPE_LONG:
            /*
             * We're OK
             */            
            break;
            
        default:
            /*
             * Whoops, it's some other kind of number
             */            
            success = FALSE;
        }	
    } else {
            IDL_tree_error(state->tree,
                           "const declaration \'%s\' must be of type short or long",
                           name);
            return FALSE;
    }

/*    if (doc_comments != NULL) {
        fputs("    ", state->file);
        printlist(state->file, doc_comments);
    }   */

    if (success) {
        /* Since Java does not have any concept of 'unsigned short', we need
         * to check that the value is in the proper range.  If not, promote
         * it to an 'int'. */
        int value = (int) IDL_INTEGER(declaration->const_exp).value;
        if (isshort && (value < -32768 || value > 32767))
            isshort = FALSE;

        fputc('\n', state->file);
        xpidl_write_comment(state, 4);
/*        write_comment(state);     */

        fprintf(state->file, "    public static final %s %s = %d;\n",
                (isshort ? "short" : "int"),
                subscriptIdentifier(state, (char*) name),
                (int) IDL_INTEGER(declaration->const_exp).value);
    } else {
        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "A constant \"%s\" was not of type short or long."
                       "  It was ignored.", name));	
    }

    return TRUE;

}

#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data))
#define ATTR_PROPS(tree) (IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data)
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL((tree)).param_type_spec)


static gboolean
attribute_declaration(TreeState *state)
{
    gboolean read_only = IDL_ATTR_DCL(state->tree).f_readonly;
    char *attribute_name = ATTR_IDENT(state->tree).str;

    gboolean method_noscript = 
        (IDL_tree_property_get(ATTR_PROPS(state->tree), "noscript") != NULL);

    gboolean method_notxpcom = 
        (IDL_tree_property_get(ATTR_PROPS(state->tree), "notxpcom") != NULL);

#if 0
    /*
     * Disabled here because I can't verify this check against possible
     * users of the java xpidl backend.
     */
    if (!verify_attribute_declaration(state->tree))
        return FALSE;
#endif

    /* Comment */
    fputc('\n', state->file);
    xpidl_write_comment(state, 4);

#if 1
    /*
     * Write access permission ("public")
     */
    fputs("    public ", state->file);
#else
    if (method_notxpcom || method_noscript)
        return TRUE;

    /*
     * Write access permission ("public" unless nonscriptable)
     */
    fputs("    ", state->file);
    if (!method_noscript) {
        fputs("public ", state->file);
    }
#endif

    /*
     * Write the proper Java return value for the get operation
     */
    if (!xpcom_to_java_type(state, ATTR_TYPE_DECL(state->tree))) {
        return FALSE;
    }
    
    /*
     * Write the name of the accessor ("get") method.
     */
    fprintf(state->file, " get%c%s();\n",
            toupper(attribute_name[0]), attribute_name + 1);


    if (!read_only) {
#if 1
        fputs("    public ", state->file);
#else
        /* Nonscriptable methods become package-protected */
        fputs("    ", state->file);
        if (!method_noscript) {
            fputs("public ", state->file);
        }
#endif

        /*
         * Write attribute access method name and return type
         */
        fprintf(state->file, "void set%c%s(",
                toupper(attribute_name[0]), 
                attribute_name+1);
        
        /*
         * Write the proper Java type for the set operation
         */
        if (!xpcom_to_java_type(state, ATTR_TYPE_DECL(state->tree))) {
            return FALSE;
        }

        /*
         * Write the name of the formal parameter.
         */
        fprintf(state->file, " a%c%s);\n", toupper(attribute_name[0]), attribute_name + 1);
    }

    return TRUE;
}


static gboolean
enum_declaration(TreeState *state)
{
    XPIDL_WARNING((state->tree, IDL_WARNING1,
                   "enums not supported, enum \'%s\' ignored",
                   IDL_IDENT(IDL_TYPE_ENUM(state->tree).ident).str));
    return TRUE;
}

backend *
xpidl_java_dispatch(void)
{
    static backend result;
    static nodeHandler table[IDLN_LAST];
    static gboolean initialized = FALSE;

    result.emit_prolog = java_prolog;
    result.emit_epilog = java_epilog;

    if (!initialized) {
        table[IDLN_INTERFACE] = interface_declaration;
        table[IDLN_LIST] = process_list;

        table[IDLN_OP_DCL] = method_declaration;
        table[IDLN_ATTR_DCL] = attribute_declaration;
        table[IDLN_CONST_DCL] = constant_declaration;

        table[IDLN_TYPE_DCL] = type_declaration;
        table[IDLN_FORWARD_DCL] = forward_declaration;

        table[IDLN_TYPE_ENUM] = enum_declaration;

        initialized = TRUE;
    }

    result.dispatch_table = table;
    return &result;
}

char* subscriptIdentifier(TreeState *state, char *str)
{
    char *sstr = NULL;                                                  
    char *keyword = g_hash_table_lookup(KEYWORDS(state), str);
    if (keyword) {
        sstr = g_strdup_printf("%s_", keyword);
        return sstr;
    }
    return str;
}

