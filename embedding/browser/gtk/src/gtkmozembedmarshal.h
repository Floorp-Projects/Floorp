
#ifndef __gtkmozembed_MARSHAL_H__
#define __gtkmozembed_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* NONE:POINTER,INT,INT (/dev/stdin:1) */
extern void gtkmozembed_VOID__POINTER_INT_INT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);
#define gtkmozembed_NONE__POINTER_INT_INT	gtkmozembed_VOID__POINTER_INT_INT

/* NONE:INT,UINT (/dev/stdin:2) */
extern void gtkmozembed_VOID__INT_UINT (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);
#define gtkmozembed_NONE__INT_UINT	gtkmozembed_VOID__INT_UINT

/* NONE:POINTER,INT,UINT (/dev/stdin:3) */
extern void gtkmozembed_VOID__POINTER_INT_UINT (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define gtkmozembed_NONE__POINTER_INT_UINT	gtkmozembed_VOID__POINTER_INT_UINT

/* NONE:POINTER,INT,POINTER (/dev/stdin:4) */
extern void gtkmozembed_VOID__POINTER_INT_POINTER (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);
#define gtkmozembed_NONE__POINTER_INT_POINTER	gtkmozembed_VOID__POINTER_INT_POINTER

G_END_DECLS

#endif /* __gtkmozembed_MARSHAL_H__ */

