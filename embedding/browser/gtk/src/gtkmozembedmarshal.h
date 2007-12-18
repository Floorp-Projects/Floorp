
#ifndef __gtkmozembed_MARSHAL_H__
#define __gtkmozembed_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOL:STRING */
extern void gtkmozembed_BOOLEAN__STRING (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING	gtkmozembed_BOOLEAN__STRING

/* BOOL:STRING,STRING */
extern void gtkmozembed_BOOLEAN__STRING_STRING (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING_STRING	gtkmozembed_BOOLEAN__STRING_STRING

/* BOOL:STRING,STRING,POINTER */
extern void gtkmozembed_BOOLEAN__STRING_STRING_POINTER (GClosure     *closure,
                                                        GValue       *return_value,
                                                        guint         n_param_values,
                                                        const GValue *param_values,
                                                        gpointer      invocation_hint,
                                                        gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING_STRING_POINTER	gtkmozembed_BOOLEAN__STRING_STRING_POINTER

/* BOOL:STRING,STRING,POINTER,INT */
extern void gtkmozembed_BOOLEAN__STRING_STRING_POINTER_INT (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING_STRING_POINTER_INT	gtkmozembed_BOOLEAN__STRING_STRING_POINTER_INT

/* BOOL:STRING,STRING,POINTER,POINTER,STRING,POINTER */
extern void gtkmozembed_BOOLEAN__STRING_STRING_POINTER_POINTER_STRING_POINTER (GClosure     *closure,
                                                                               GValue       *return_value,
                                                                               guint         n_param_values,
                                                                               const GValue *param_values,
                                                                               gpointer      invocation_hint,
                                                                               gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING_STRING_POINTER_POINTER_STRING_POINTER	gtkmozembed_BOOLEAN__STRING_STRING_POINTER_POINTER_STRING_POINTER

/* BOOL:STRING,STRING,POINTER,STRING,POINTER */
extern void gtkmozembed_BOOLEAN__STRING_STRING_POINTER_STRING_POINTER (GClosure     *closure,
                                                                       GValue       *return_value,
                                                                       guint         n_param_values,
                                                                       const GValue *param_values,
                                                                       gpointer      invocation_hint,
                                                                       gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING_STRING_POINTER_STRING_POINTER	gtkmozembed_BOOLEAN__STRING_STRING_POINTER_STRING_POINTER

/* BOOL:STRING,STRING,STRING,POINTER */
extern void gtkmozembed_BOOLEAN__STRING_STRING_STRING_POINTER (GClosure     *closure,
                                                               GValue       *return_value,
                                                               guint         n_param_values,
                                                               const GValue *param_values,
                                                               gpointer      invocation_hint,
                                                               gpointer      marshal_data);
#define gtkmozembed_BOOL__STRING_STRING_STRING_POINTER	gtkmozembed_BOOLEAN__STRING_STRING_STRING_POINTER

/* INT:STRING,INT,INT,INT,INT,INT */
extern void gtkmozembed_INT__STRING_INT_INT_INT_INT_INT (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);

/* INT:STRING,STRING,INT,INT,INT,INT */
extern void gtkmozembed_INT__STRING_STRING_INT_INT_INT_INT (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* INT:STRING,STRING,UINT,STRING,STRING,STRING,STRING,POINTER */
extern void gtkmozembed_INT__STRING_STRING_UINT_STRING_STRING_STRING_STRING_POINTER (GClosure     *closure,
                                                                                     GValue       *return_value,
                                                                                     guint         n_param_values,
                                                                                     const GValue *param_values,
                                                                                     gpointer      invocation_hint,
                                                                                     gpointer      marshal_data);

/* INT:VOID */
extern void gtkmozembed_INT__VOID (GClosure     *closure,
                                   GValue       *return_value,
                                   guint         n_param_values,
                                   const GValue *param_values,
                                   gpointer      invocation_hint,
                                   gpointer      marshal_data);

/* STRING:STRING,STRING */
extern void gtkmozembed_STRING__STRING_STRING (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* VOID:BOOL */
#define gtkmozembed_VOID__BOOLEAN	g_cclosure_marshal_VOID__BOOLEAN
#define gtkmozembed_VOID__BOOL	gtkmozembed_VOID__BOOLEAN

/* VOID:INT,INT,BOOL */
extern void gtkmozembed_VOID__INT_INT_BOOLEAN (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);
#define gtkmozembed_VOID__INT_INT_BOOL	gtkmozembed_VOID__INT_INT_BOOLEAN

/* VOID:INT,STRING */
extern void gtkmozembed_VOID__INT_STRING (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* VOID:INT,STRING,STRING */
extern void gtkmozembed_VOID__INT_STRING_STRING (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* VOID:INT,UINT */
extern void gtkmozembed_VOID__INT_UINT (GClosure     *closure,
                                        GValue       *return_value,
                                        guint         n_param_values,
                                        const GValue *param_values,
                                        gpointer      invocation_hint,
                                        gpointer      marshal_data);

/* VOID:POINTER,INT,POINTER */
extern void gtkmozembed_VOID__POINTER_INT_POINTER (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* VOID:POINTER,INT,STRING,STRING,STRING,STRING,STRING,BOOLEAN,INT */
extern void gtkmozembed_VOID__POINTER_INT_STRING_STRING_STRING_STRING_STRING_BOOLEAN_INT (GClosure     *closure,
                                                                                          GValue       *return_value,
                                                                                          guint         n_param_values,
                                                                                          const GValue *param_values,
                                                                                          gpointer      invocation_hint,
                                                                                          gpointer      marshal_data);

/* VOID:POINTER,STRING,BOOL,BOOL */
extern void gtkmozembed_VOID__POINTER_STRING_BOOLEAN_BOOLEAN (GClosure     *closure,
                                                              GValue       *return_value,
                                                              guint         n_param_values,
                                                              const GValue *param_values,
                                                              gpointer      invocation_hint,
                                                              gpointer      marshal_data);
#define gtkmozembed_VOID__POINTER_STRING_BOOL_BOOL	gtkmozembed_VOID__POINTER_STRING_BOOLEAN_BOOLEAN

/* VOID:STRING,INT,INT */
extern void gtkmozembed_VOID__STRING_INT_INT (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:STRING,INT,UINT */
extern void gtkmozembed_VOID__STRING_INT_UINT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

/* VOID:STRING,STRING */
extern void gtkmozembed_VOID__STRING_STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:STRING,STRING,POINTER */
extern void gtkmozembed_VOID__STRING_STRING_POINTER (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

/* VOID:STRING,STRING,STRING,ULONG,INT */
extern void gtkmozembed_VOID__STRING_STRING_STRING_ULONG_INT (GClosure     *closure,
                                                              GValue       *return_value,
                                                              guint         n_param_values,
                                                              const GValue *param_values,
                                                              gpointer      invocation_hint,
                                                              gpointer      marshal_data);

/* VOID:STRING,STRING,STRING,POINTER */
extern void gtkmozembed_VOID__STRING_STRING_STRING_POINTER (GClosure     *closure,
                                                            GValue       *return_value,
                                                            guint         n_param_values,
                                                            const GValue *param_values,
                                                            gpointer      invocation_hint,
                                                            gpointer      marshal_data);

/* VOID:UINT,INT,INT,STRING,STRING,STRING,STRING */
extern void gtkmozembed_VOID__UINT_INT_INT_STRING_STRING_STRING_STRING (GClosure     *closure,
                                                                        GValue       *return_value,
                                                                        guint         n_param_values,
                                                                        const GValue *param_values,
                                                                        gpointer      invocation_hint,
                                                                        gpointer      marshal_data);

/* VOID:ULONG,ULONG,ULONG */
extern void gtkmozembed_VOID__ULONG_ULONG_ULONG (GClosure     *closure,
                                                 GValue       *return_value,
                                                 guint         n_param_values,
                                                 const GValue *param_values,
                                                 gpointer      invocation_hint,
                                                 gpointer      marshal_data);

/* BOOL:POINTER,UINT */
extern void gtkmozembed_BOOLEAN__POINTER_UINT (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);
#define gtkmozembed_BOOL__POINTER_UINT	gtkmozembed_BOOLEAN__POINTER_UINT

/* VOID:POINTER */
#define gtkmozembed_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* BOOL:STRING,STRING,POINTER */

G_END_DECLS

#endif /* __gtkmozembed_MARSHAL_H__ */

