(progn
  (defparameter *nw*
    (generate-world
     "N"
     '((grammar name-resolution-grammar :lalr-1 :start)
       
       (production :start () start-none)
       
       (deftype value (oneof null abstract-value))
       (deftype class (oneof abstract-class))
       (deftype type (oneof abstract-type))
       (deftype namespace (oneof abstract-namespace))
       (deftype scope (oneof abstract-scope))

       (deftype getter (-> (value) value))
       (deftype setter (-> (value value) value))

       (%section "Namespaces")

       (define (create-namespace (supernamespaces (vector namespace))) namespace
         (bottom))

       (%section "Classes and Intefaces")

       (define (create-class (interface boolean) (superclasses (vector class)) (implementees (vector class))) class
         (bottom))

       (define (create-uninitialized-instance-slot (c class) (t type)) (tuple (get getter) (set setter))
         (bottom))

       (define (create-instance-slot (c class) (t type) (initial-value value)) (tuple (get getter) (set setter))
         (bottom))

       (define (freeze-instance-slots (c class)) void
         (bottom))

       (define (create-instance (c class)) value
         (bottom))

       (%section "Members")

       (define (add-getter-member (visibility scope) (n namespace) (c class) (name string) (g getter)) void
         (bottom))
       (define (add-setter-member (visibility scope) (n namespace) (c class) (name string) (s setter)) void
         (bottom))

       (define (lookup-getter-member (s scope) (n namespace) (v value) (name string)) getter
         (bottom))
       (define (lookup-setter-member (s scope) (n namespace) (v value) (name string)) setter
         (bottom))
       )))
  
  (defparameter *ng* (world-grammar *nw* 'name-resolution-grammar)))

#|
(depict-rtf-to-local-file
 "Test/NameResolutionSemantics.rtf"
 "Name Resolution Semantics"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *nw*)))
|#

(depict-html-to-local-file
 "Test/NameResolutionSemantics.html"
 "Name Resolution Semantics"
 t
 #'(lambda (html-stream)
     (depict-world-commands html-stream *nw*))
 :external-link-base "")

(length (grammar-states *ng*))
