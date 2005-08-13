
#ifndef __gtkmozembed_MARSHAL_H__
#define __gtkmozembed_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOL:STRING (types.txt:1) */
extern void gtkmozembed_BOOLEAN__STRING (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING	gtkmozembed_BOOLEAN__STRING

/* VOID:STRING,INT,INT (types.txt:2) */
extern void gtkmozembed_VOID__STRING_INT_INT (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:INT,UINT (types.txt:3) */
extern void gtkmozembed_VOID__INT_UINT (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);

/* VOID:STRING,INT,UINT (types.txt:4) */
extern void gtkmozembed_VOID__STRING_INT_UINT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* VOID:POINTER,INT,POINTER (types.txt:5) */
extern void gtkmozembed_VOID__POINTER_INT_POINTER (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

G_END_DECLS

#endif /* __gtkmozembed_MARSHAL_H__ */

