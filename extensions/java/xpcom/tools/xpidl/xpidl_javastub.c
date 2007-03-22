/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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

#define COUNT_METHODS

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
	int numMethods;
	gboolean bHasBaseClass;
	gboolean bCountingMethods;
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

#ifdef COUNT_METHODS
	PRIVDATA(state)->numMethods = 0;
	PRIVDATA(state)->bHasBaseClass = FALSE; 
	PRIVDATA(state)->bCountingMethods = FALSE;
#endif

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
#ifdef COUNT_METHODS
	char *baseclass_name = NULL;
#endif

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
    strcat(outname, "_Stub.java");

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

    if (state->package) {
        fprintf(state->file, "\npackage %s.stubs;\n\n", state->package);
        fprintf(state->file, "import %s.*;\n", state->package);
    }

    if (!state->package || strcmp(state->package, "org.mozilla.xpcom") != 0)
        fputs("import org.mozilla.xpcom.*;\n", state->file);

    fputs("\n", state->file);

    /*
     * Write "public class <foo>"
     */

     fprintf(state->file, "public class %s_Stub", interface_name);

    /*
     * Check for inheritence, and iterator over the inherited names,
     * if any.
     */

    if ((iterator = IDL_INTERFACE(interface).inheritance_spec)) {
        fputs(" extends ", state->file);

        do {
#ifdef COUNT_METHODS
            if (baseclass_name == NULL) {
                baseclass_name = IDL_IDENT(IDL_LIST(iterator).data).str;
                PRIVDATA(state)->bHasBaseClass = TRUE;
            }
#endif

            fprintf(state->file, "%s_Stub", 
                    IDL_IDENT(IDL_LIST(iterator).data).str);
	    
            if (IDL_LIST(iterator).next) {
                fputs(", ", state->file);
            }
        } while ((iterator = IDL_LIST(iterator).next));
    } else {
        fputs(" extends Object", state->file);
    }

    fprintf(state->file, " implements %s\n{\n", interface_name);

#ifdef COUNT_METHODS
	/*
     * Count number of methods
     */
    
    state->tree = IDL_INTERFACE(interface).body;

	PRIVDATA(state)->bCountingMethods = TRUE;
	PRIVDATA(state)->numMethods = 0;

    if (state->tree && !xpidl_process_node(state)) {
        return FALSE;
    }	

    if (PRIVDATA(state)->numMethods) {
        fputs("\n    static final int LAST_METHOD_ID = ", state->file);
        if (!PRIVDATA(state)->bHasBaseClass) 
            fprintf(state->file, "%d;\n\n",PRIVDATA(state)->numMethods - 1);
        else
            fprintf(state->file, "%s_Stub.LAST_METHOD_ID + %d;\n\n", baseclass_name,
                    PRIVDATA(state)->numMethods);
    }

	state->tree = interface;
#endif

    /*
     * Check for inheritence.
     */
#if !defined(COUNT_METHODS)
	PRIVDATA(state)->bHasBaseClass = FALSE; 
    if ((iterator = IDL_INTERFACE(interface).inheritance_spec)) {
        PRIVDATA(state)->bHasBaseClass = TRUE;
    }
#endif

    /*
     * Advance the state of the tree, go on to process more
     */
    
    state->tree = IDL_INTERFACE(interface).body;

	PRIVDATA(state)->bCountingMethods = FALSE;
	PRIVDATA(state)->numMethods = 0;

    if (state->tree && !xpidl_process_node(state)) {
        return FALSE;
    }

    /*
     * Create the finalize() method
     */
    fputs("    protected void finalize() throws Throwable {\n", state->file);
    fputs("        XPCOMPrivate.FinalizeStub(this);\n    }", state->file);
/*    fputs("        try {\n", state->file);
    fputs("            XPCOM.FinalizeStub(this);\n", state->file);
    fputs("        } finally {\n            super.finalize();\n", state->file);
    fputs("        }\n    }", state->file);*/

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

typedef enum JavaNativeType {
    JAVA_NATIVE_ERROR,      /* error condition */
    JAVA_BYTE,
    JAVA_SHORT,
    JAVA_INT,
    JAVA_LONG,
    JAVA_FLOAT,
    JAVA_DOUBLE,
    JAVA_CHAR,
    JAVA_BOOL,
    JAVA_BYTEARRAY,
    JAVA_STRING,
    JAVA_NSISUPPORTS,
    JAVA_OBJECT
} JavaNativeType;

static JavaNativeType
xpcom_to_java_type2(TreeState *state, IDL_tree type)
{
    IDL_tree real_type;
    IDL_tree up;

    if (!type) {
        return JAVA_OBJECT;
    }

    /* Could be a typedef; try to map it to the real type */
    real_type = find_underlying_type(type);
    type = real_type ? real_type : type;

    switch(IDL_NODE_TYPE(type)) {
      case IDLN_TYPE_INTEGER: {
        switch(IDL_TYPE_INTEGER(type).f_type) {
          case IDL_INTEGER_TYPE_SHORT:
              return JAVA_SHORT;
              break;
          case IDL_INTEGER_TYPE_LONG:
              return JAVA_INT;
              break;
          case IDL_INTEGER_TYPE_LONGLONG:
              return JAVA_LONG;
              break;
          default:
              g_error("Unknown integer type: %d\n",
                      IDL_TYPE_INTEGER(type).f_type);
              return JAVA_NATIVE_ERROR;
        }
        break;
      }

      case IDLN_TYPE_CHAR:
      case IDLN_TYPE_WIDE_CHAR:
        return JAVA_CHAR;
        break;

      case IDLN_TYPE_WIDE_STRING:
      case IDLN_TYPE_STRING:
        return JAVA_STRING;
        break;

      case IDLN_TYPE_BOOLEAN:
        return JAVA_BOOL;
        break;

      case IDLN_TYPE_OCTET:
        return JAVA_BYTE;
        break;

      case IDLN_TYPE_FLOAT:
        switch(IDL_TYPE_FLOAT(type).f_type) {
          case IDL_FLOAT_TYPE_FLOAT:
            return JAVA_FLOAT;
            break;
          case IDL_FLOAT_TYPE_DOUBLE:
            return JAVA_DOUBLE;
            break;
          default:
            g_error("Unknown floating point type: %d\n",
                    IDL_TYPE_FLOAT(type).f_type);
            return JAVA_NATIVE_ERROR;
            break;
        }
        break;

      case IDLN_IDENT:
        if (!(up = IDL_NODE_UP(type))) {
            IDL_tree_error(state->tree,
                           "ERROR: orphan ident %s in param list\n",
                           IDL_IDENT(state->tree).str);
            return JAVA_NATIVE_ERROR;
        }
        switch (IDL_NODE_TYPE(up)) {
            /* This whole section is abominably ugly */
          case IDLN_FORWARD_DCL:
          case IDLN_INTERFACE: {
            return JAVA_NSISUPPORTS;
            break;
          }
          case IDLN_NATIVE: {
              if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL &&
                  IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                          "iid_is"))
                  return JAVA_NSISUPPORTS;

              if (IDL_tree_property_get(type, "nsid")) {
                  return JAVA_STRING;
              } else if (IDL_tree_property_get(type, "domstring")) {
                  return JAVA_STRING;
              } else if (IDL_tree_property_get(type, "astring")) {
                  return JAVA_STRING;
              } else if (IDL_tree_property_get(type, "utf8string")) {
                  return JAVA_STRING;
              } else if (IDL_tree_property_get(type, "cstring")) {
                  return JAVA_STRING;
              } else {
                  const char* user_type = IDL_NATIVE(IDL_NODE_UP(type)).user_type;
                  IDL_tree real_type = 
                      g_hash_table_lookup(TYPEDEFS(state), user_type);

                  if (real_type) {
                      return xpcom_to_java_type2(state, real_type);
                  } else {
                      if (strcmp(user_type, "PRInt8") == 0 ||
                          strcmp(user_type, "PRUint8") == 0) {
                          return JAVA_BYTE;
                      } else if (strcmp(user_type, "PRInt16") == 0 ||
                                 strcmp(user_type, "PRUint16") == 0) {
                          return JAVA_SHORT;
                      } else if (strcmp(user_type, "PRInt32") == 0 ||
                                 strcmp(user_type, "PRUint32") == 0 ||
                                 strcmp(user_type, "int") == 0) {
                          return JAVA_INT;
                      } else if (strcmp(user_type, "PRInt64") == 0 ||
                                 strcmp(user_type, "PRUint64") == 0) {
                          return JAVA_LONG;
                      } else if (strcmp(user_type, "PRBool") == 0) {
                          return JAVA_BOOL;
                      } else if (strncmp(user_type, "char", 4) == 0 ||
                                 strncmp(user_type, "const char", 10) == 0 ||
                                 strncmp(user_type, "unsigned char", 13) == 0) {
                          if (IDL_tree_property_get(type, "ptr")) {
                              return JAVA_BYTEARRAY;
                          } else {
                              return JAVA_CHAR;
                          }
                      } else if (strcmp(user_type, "nsIID") == 0) {
                          return JAVA_STRING;
                      } else if (strcmp(user_type, "nsString") == 0 ||
                                 strcmp(user_type, "nsAString") == 0 ||
                                 strcmp(user_type, "nsACString") == 0) {
                          return JAVA_STRING;
                      } else {
                          return JAVA_INT;
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
                    return xpcom_to_java_type2(state, new_type);
                } else {
                    /* do what we would do in recursion if !type */
                    return JAVA_OBJECT;
                }
            }
            IDL_tree_error(state->tree,
                           "can't handle %s ident in param list\n",
                               /* XXX is this safe to use on Win now? */
                               IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
/*                           "that type of"*/
                           );
            return JAVA_NATIVE_ERROR;
        }
        break;

    default:
      IDL_tree_error(state->tree, "can't handle %s in param list\n",
                     /* XXX is this safe to use on Win now? */
                     IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
/*                  "that type"*/
      );
      return JAVA_NATIVE_ERROR;
    }

    return JAVA_NATIVE_ERROR;
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
                         "can't handle %s ident in param list [1]\n",
                               /* XXX is this safe to use on Win now? */
                               IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
/*                         "that type of"*/
                         );
          return FALSE;
      }
      break;

    default:
      IDL_tree_error(state->tree, "can't handle %s in param list [1]\n",
                     /* XXX is this safe to use on Win now? */
                     IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
/*                  "that type"*/
      );
      return FALSE;
    }

    return TRUE;

}

static gboolean
xpcom_to_java_object_type(TreeState *state, IDL_tree type, char* parmName, gboolean is_object)
{
    JavaNativeType jtype = xpcom_to_java_type2(state, type);
    
    switch (jtype) {
        case JAVA_BYTE:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Byte(%s)", parmName);
            }
            break;
        case JAVA_SHORT:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Short(%s)", parmName);
            }
            break;
        case JAVA_INT:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Integer(%s)", parmName);
            }
            break;
        case JAVA_LONG:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Long(%s)", parmName);
            }
            break;
        case JAVA_FLOAT:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Float(%s)", parmName);
            }
            break;
        case JAVA_DOUBLE:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Double(%s)", parmName);
            }
            break;
        case JAVA_CHAR:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Character(%s)", parmName);
            }
            break;
        case JAVA_BOOL:
            if (is_object) {
                fputs(parmName, state->file);
            } else {
                fprintf(state->file, "new Boolean(%s)", parmName);
            }
            break;
        case JAVA_BYTEARRAY:
        case JAVA_STRING:
        case JAVA_OBJECT:
        case JAVA_NSISUPPORTS:
            fputs(parmName, state->file);
            break;
        default:
            g_error("Unknown java type\n");
            return FALSE;
            break;
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
xpcom_to_java_object(TreeState *state) 
{
    char* paramName;
    gboolean is_object;

    /*
     * Write Java object param
     */
    paramName = IDL_IDENT(IDL_PARAM_DCL(state->tree).simple_declarator).str;
    is_object = IDL_PARAM_DCL(state->tree).attr != IDL_PARAM_IN ||
                IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator, "array");
    if (!xpcom_to_java_object_type(state,
                                   IDL_PARAM_DCL(state->tree).param_type_spec,
                                   paramName, is_object)) {
        return FALSE;
    }
    
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

#ifdef COUNT_METHODS
	if (PRIVDATA(state)->bCountingMethods) {
		return TRUE;
	}
#endif

    /* XXX: check for illegal types */

    g_hash_table_insert(TYPEDEFS(state),
                        IDL_IDENT(IDL_LIST(dcls).data).str,
                        type);

    return TRUE;
}

static gboolean
method_declaration(TreeState *state) 
{
	gboolean bHasOutParm = FALSE; 
	gboolean bHasReturnValue = TRUE; 
	char *parmBuffer = NULL;
	IDL_tree orig_tree = NULL;
    GSList *doc_comments = IDL_IDENT(IDL_OP_DCL(state->tree).ident).comments;
    int numMethods;
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

#ifdef COUNT_METHODS
	if (PRIVDATA(state)->bCountingMethods) {
		PRIVDATA(state)->numMethods++;
		return TRUE;
	}
#endif

#if 1
    /* do not write non-xpcom methods */
    if (method_notxpcom) {
        xpidl_write_comment(state, 4);
        fputc('\n', state->file);
        PRIVDATA(state)->numMethods++;
        return TRUE;
    }

    /*
     * Write beginning of method declaration
     */
    fputs("    public ", state->file);
#else
    /* do not write nonscriptable methods */
    if (method_notxpcom || method_noscript) {
        xpidl_write_comment(state, 4);
        fputc('\n', state->file);
        PRIVDATA(state)->numMethods++;
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

/*    if (method->raises_expr) {
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

    fputs(" {\n        ", state->file);
    
    /*
     * If method has a return value, then we will return what we
     * get from CallXPCOMMethod()
     */
    if (method->op_type_spec || retval_param)
        fputs("return ", state->file);
    
    /*
     * Create CallXPCOMMethod invocation
     */
    if (method->op_type_spec || retval_param) {
        IDL_tree list = NULL;
        const char* is_array = NULL;
        JavaNativeType jtype = JAVA_NATIVE_ERROR;

        if (method->op_type_spec) {
            jtype = xpcom_to_java_type2(state, method->op_type_spec);
        } else {
            IDL_tree original_tree = state->tree;
            state->tree = IDL_LIST(retval_param).data;
            jtype = xpcom_to_java_type2(state,
                                        IDL_PARAM_DCL(state->tree).param_type_spec);
            state->tree = original_tree;
        }

        if (jtype == JAVA_NATIVE_ERROR)
            return FALSE;

        /* is return type an array? */
        if (retval_param) {
            list = IDL_LIST(retval_param).data;
            is_array = IDL_tree_property_get(IDL_PARAM_DCL(list).simple_declarator,
                                             "array");
        }

        switch (jtype) {
            case JAVA_BYTE:
                fputs("XPCOMPrivate.CallXPCOMMethodByte", state->file);
                break;
            case JAVA_BYTEARRAY:
                fputs("(byte[]) XPCOMPrivate.CallXPCOMMethodObj", state->file);
                break;
            case JAVA_SHORT:
                fputs("XPCOMPrivate.CallXPCOMMethodShort", state->file);
                break;
            case JAVA_INT:
                fputs("XPCOMPrivate.CallXPCOMMethodInt", state->file);
                break;
            case JAVA_LONG:
                fputs("XPCOMPrivate.CallXPCOMMethodLong", state->file);
                break;
            case JAVA_FLOAT:
                fputs("XPCOMPrivate.CallXPCOMMethodFloat", state->file);
                break;
            case JAVA_DOUBLE:
                fputs("XPCOMPrivate.CallXPCOMMethodDouble", state->file);
                break;
            case JAVA_CHAR:
                fputs("XPCOMPrivate.CallXPCOMMethodChar", state->file);
                break;
            case JAVA_BOOL:
                fputs("XPCOMPrivate.CallXPCOMMethodBool", state->file);
                break;
            case JAVA_STRING:
                fputs("(String", state->file);
                if (is_array)
                    fputs("[]", state->file);
                fputs(") XPCOMPrivate.CallXPCOMMethodObj", state->file);
                break;
            case JAVA_NSISUPPORTS:
                fputs("(", state->file);
                if (method->op_type_spec) {
                    if (!xpcom_to_java_type(state, method->op_type_spec))
                        return FALSE;
                } else {
                    IDL_tree original_tree = state->tree;
                    state->tree = IDL_LIST(retval_param).data;
                    if (!xpcom_to_java_type(state, IDL_PARAM_DCL(state->tree).param_type_spec))
                      return FALSE;
                    state->tree = original_tree;
                }
                if (is_array)
                    fputs("[]", state->file);
                fputs(") XPCOMPrivate.CallXPCOMMethodObj", state->file);
                break;
            case JAVA_OBJECT:
                fputs("XPCOMPrivate.CallXPCOMMethodObj", state->file);
                break;
            default:
                g_error("Unknown jtype: %d\n", jtype);
                return FALSE;
        }

        if (is_array)
            fputs("A", state->file);

        state->tree = orig_tree;
    } else {
        fputs("XPCOMPrivate.CallXPCOMMethodVoid", state->file);
    }
    fputs("(this, ", state->file);

    numMethods = PRIVDATA(state)->numMethods;
    if (PRIVDATA(state)->bHasBaseClass) {
        numMethods++;
        fputs("super.LAST_METHOD_ID + ", state->file);
    }
    fprintf(state->file, "%d, ", numMethods);

    /*
     * Write parameters
     */
    for (iterator = method->parameter_dcls; iterator != NULL; 
         iterator = IDL_LIST(iterator).next) {

        /* Skip "retval" */
        if (iterator == retval_param) {
            if (iterator == method->parameter_dcls)
                fputs("null", state->file);
            continue;
        }

        /* Start of object array decl */
        if (iterator == method->parameter_dcls) {
          fputs("new Object[] { ", state->file);
        }

        if (iterator != method->parameter_dcls) {
            fputs(", ", state->file);
        }

        state->tree = IDL_LIST(iterator).data;

        if (!xpcom_to_java_object(state)) {
            return FALSE;
        }

        /* Close object array decl */
        if (IDL_LIST(iterator).next == NULL ||
            (IDL_LIST(iterator).next == retval_param &&
             IDL_LIST(retval_param).next == NULL))
            fputs(" }", state->file);
    }

    if (method->parameter_dcls == NULL) {
        fputs("null", state->file);
    }

    fputs(");\n    }\n\n", state->file);

	PRIVDATA(state)->numMethods++;

	return TRUE;
}


static gboolean
constant_declaration(TreeState *state)
{
    /* Don't output constants for stubs */
    return TRUE;

}

#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data))
#define ATTR_PROPS(tree) (IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data)
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL((tree)).param_type_spec)


static gboolean
attribute_declaration(TreeState *state)
{
    char* attribute_text;
    char* name;
    int numMethods;
    JavaNativeType jtype;

    gboolean read_only = IDL_ATTR_DCL(state->tree).f_readonly;
    char *attribute_name = ATTR_IDENT(state->tree).str;

    gboolean method_noscript = 
        (IDL_tree_property_get(ATTR_PROPS(state->tree), "noscript") != NULL);

    gboolean method_notxpcom = 
        (IDL_tree_property_get(ATTR_PROPS(state->tree), "notxpcom") != NULL);

#ifdef COUNT_METHODS
	if (PRIVDATA(state)->bCountingMethods) {
		PRIVDATA(state)->numMethods++;
		if (!read_only) 
			PRIVDATA(state)->numMethods++;

		return TRUE;
	}
#endif

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
    fprintf(state->file, " get%c%s()\n",
            toupper(attribute_name[0]), attribute_name + 1);

    fputs("    {\n        ", state->file);

    jtype = xpcom_to_java_type2(state, ATTR_TYPE_DECL(state->tree));
    if (jtype == JAVA_NATIVE_ERROR)
        return FALSE;
    fputs("return ", state->file);
    switch (jtype) {
        case JAVA_BYTE:
            fputs("XPCOMPrivate.CallXPCOMMethodByte", state->file);
            break;
        case JAVA_BYTEARRAY:
            fputs("(byte[]) XPCOMPrivate.CallXPCOMMethodObj", state->file);
            break;
        case JAVA_SHORT:
            fputs("XPCOMPrivate.CallXPCOMMethodShort", state->file);
            break;
        case JAVA_INT:
            fputs("XPCOMPrivate.CallXPCOMMethodInt", state->file);
            break;
        case JAVA_LONG:
            fputs("XPCOMPrivate.CallXPCOMMethodLong", state->file);
            break;
        case JAVA_FLOAT:
            fputs("XPCOMPrivate.CallXPCOMMethodFloat", state->file);
            break;
        case JAVA_DOUBLE:
            fputs("XPCOMPrivate.CallXPCOMMethodDouble", state->file);
            break;
        case JAVA_CHAR:
            fputs("XPCOMPrivate.CallXPCOMMethodChar", state->file);
            break;
        case JAVA_BOOL:
            fputs("XPCOMPrivate.CallXPCOMMethodBool", state->file);
            break;
        case JAVA_STRING:
            fputs("(String) XPCOMPrivate.CallXPCOMMethodObj", state->file);
            break;
        case JAVA_NSISUPPORTS:
            fputs("(", state->file);
            if (!xpcom_to_java_type(state, ATTR_TYPE_DECL(state->tree))) {
                return FALSE;
            }
            fputs(") XPCOMPrivate.CallXPCOMMethodObj", state->file);
            break;
        case JAVA_OBJECT:
            fputs("XPCOMPrivate.CallXPCOMMethodObj", state->file);
            break;
        default:
            g_error("Unknown jtype: %d\n", jtype);
            return FALSE;
    }
    fputs("(this, ", state->file);

    numMethods = PRIVDATA(state)->numMethods;
    if (PRIVDATA(state)->bHasBaseClass) {
        numMethods++;
        fputs("super.LAST_METHOD_ID + ", state->file);
    }
    fprintf(state->file, "%d, null);\n", numMethods);

	fputs("    }\n\n", state->file);

	PRIVDATA(state)->numMethods++;

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
        fputs(" value)\n", state->file);
/*        fprintf(state->file, " a%c%s)\n", toupper(attribute_name[0]), attribute_name + 1);*/
        
        fputs("    {\n        ", state->file);

        /*
         * Write CallXPCOMMethod invocation
         */
        fputs("XPCOMPrivate.CallXPCOMMethodVoid(this, ", state->file);
        numMethods = PRIVDATA(state)->numMethods;
		if (PRIVDATA(state)->bHasBaseClass) {
            numMethods++;
            fputs("super.LAST_METHOD_ID + ", state->file);
        }
        fprintf(state->file, "%d, ", numMethods);
        
        /*
         * Write parameter
         */
        fputs("new Object[] { ", state->file);
/*        if (!xpcom_to_java_object(state)) {
            return FALSE;
        }   */
        if (!xpcom_to_java_object_type(state, ATTR_TYPE_DECL(state->tree), "value", 0)) {
            return FALSE;
        }
        fputs(" });", state->file);

		fputs("\n    }\n\n", state->file);

		PRIVDATA(state)->numMethods++;
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
xpidl_javastub_dispatch(void)
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

