;;;
;;; JavaScript 2.0 parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(defparameter *jw-source* 
  '((line-grammar code-grammar :lr-1 :program)
    
    (%heading (1 :semantics) "Data Model")
    (%heading (2 :semantics) "Errors")
    
    (deftag syntax-error)
    (deftag reference-error)
    (deftag type-error)
    (deftag property-not-found-error)
    (deftag argument-mismatch-error)
    (deftype semantic-error (tag syntax-error reference-error type-error property-not-found-error argument-mismatch-error))
    
    (deftuple go-break (value object) (label string))
    (deftuple go-continue (value object) (label string))
    (deftuple go-return (value object))
    (deftuple go-throw (value object))
    (deftype early-exit (union go-break go-continue go-return go-throw))
    
    (deftype semantic-exception (union early-exit semantic-error))
    
    
    (%heading (2 :semantics) "Objects")
    (deftype object (union undefined null boolean float64 string namespace attribute class method-closure prototype instance))
    
    
    (%heading (3 :semantics) "Undefined")
    (deftag undefined)
    (deftype undefined (tag undefined))
    
    
    (%heading (3 :semantics) "Null")
    (deftag null)
    (deftype null (tag null))
    
    
    (%heading (3 :semantics) "Namespaces")
    (defrecord namespace (name string))
    (deftype namespace-opt (union null namespace))
    
    (define public-namespace namespace (new namespace "public"))
    
    
    (%heading (3 :semantics) "Attributes")
    (deftag dynamic)
    (deftag fixed)
    (deftype class-modifier (tag null dynamic fixed))
    
    (deftag static)
    (deftag constructor)
    (deftag operator)
    (deftag abstract)
    (deftag virtual)
    (deftag final)
    (deftype member-modifier (tag null static constructor operator abstract virtual final))
    
    (deftag may-override)
    (deftag override)
    (deftype override-modifier (tag null may-override override))
    
    (deftuple attribute
      (namespaces (list-set namespace))
      (local boolean)
      (extend class-opt)
      (enumerable boolean)
      (class-mod class-modifier)
      (member-mod member-modifier)
      (override-mod override-modifier)
      (prototype boolean)
      (unused boolean))
    
    
    (%heading (3 :semantics) "Classes")
    (defrecord class
      (super class-opt)
      (prototype object)
      (global-members (list-set global-member) :var)
      (instance-members (list-set instance-member) :var)
      (class-mod class-modifier)
      (primitive boolean)
      (private-namespace namespace)
      (call invoker)
      (construct invoker))
    (deftype class-opt (union null class))
    
    (define (make-built-in-class (superclass class-opt) (class-mod class-modifier) (primitive boolean)) class
      (const private-namespace namespace (new namespace "private"))
      (function (call (this object :unused) (args argument-list :unused)) object
        (todo))
      (function (construct (this object :unused) (args argument-list :unused)) object
        (todo))
      (return (new class superclass null (list-set-of global-member) (list-set-of instance-member)
                   class-mod primitive private-namespace call construct)))
    
    (define object-class class (make-built-in-class null dynamic true))
    (define undefined-class class (make-built-in-class object-class fixed true))
    (define null-class class (make-built-in-class object-class fixed true))
    (define boolean-class class (make-built-in-class object-class fixed true))
    (define number-class class (make-built-in-class object-class fixed true))
    (define string-class class (make-built-in-class object-class fixed false))
    (define character-class class (make-built-in-class string-class fixed false))
    (define namespace-class class (make-built-in-class object-class fixed false))
    (define attribute-class class (make-built-in-class object-class fixed false))
    (define class-class class (make-built-in-class object-class fixed false))
    (define function-class class (make-built-in-class object-class fixed false))
    (define prototype-class class (make-built-in-class object-class fixed false))
    
    (%text :comment "Return an ordered list of class " (:local d) :apostrophe "s ancestors, including " (:local d) " itself.")
    (define (ancestors (c class)) (vector class)
      (const s class-opt (& super c))
      (if (in s (tag null) :narrow-false)
        (return (vector c))
        (return (append (ancestors s) (vector c)))))
    
    (%text :comment "Return " (:tag true) " if " (:local c) " is " (:local d) " or an ancestor of " (:local d) ".")
    (define (is-ancestor (c class) (d class)) boolean
      (cond
       ((= c d class) (return true))
       (nil (const s class-opt (& super d))
            (rwhen (in s (tag null) :narrow-false)
              (return false))
            (return (is-ancestor c s)))))
    
    (%text :comment "Return " (:tag true) " if " (:local c) " is an ancestor of " (:local d) " other than " (:local d) " itself.")
    (define (is-proper-ancestor (c class) (d class)) boolean
      (return (and (is-ancestor c d) (/= c d class))))
    
    
    (%heading (4 :semantics) "Members")
    (deftype instance-category (tag abstract virtual final))
    (deftype instance-data (union slot-id method accessor))
    
    (defrecord instance-member 
      (name string)
      (namespaces (list-set namespace))
      (category instance-category)
      (readable boolean)
      (writable boolean)
      (indexable boolean)
      (enumerable boolean)
      (data (union instance-data namespace)))
    
    (deftype global-category (tag static constructor))
    (deftype global-data (union global-slot method accessor))
    
    (defrecord global-member
      (name string)
      (namespaces (list-set namespace))
      (category global-category)
      (readable boolean)
      (writable boolean)
      (indexable boolean)
      (enumerable boolean)
      (data (union global-data namespace)))
    (deftype member (union instance-member global-member))
    (deftype member-data (union instance-data global-data))
    (deftype member-data-opt (union null member-data))
    
    (defrecord global-slot
      (type class)
      (value object :var))
    
    (defrecord method
      (type signature)
      (f instance-opt)) ;Method code (may be null)
    
    (defrecord accessor
      (type class)
      (f instance)) ;Getter or setter function code
    
        
    (%heading (3 :semantics) "Method Closures")
    (deftuple method-closure
      (this object)
      (method method))
    
    
    (%heading (3 :semantics) "Prototype Instances")
    (defrecord prototype
      (parent prototype-opt)
      (dynamic-properties (list-set dynamic-property) :var))
    (deftype prototype-opt (union null prototype))
    
    
    (%heading (3 :semantics) "Class Instances")
    (defrecord instance
      (type class)
      (call invoker)
      (construct invoker)
      (typeof-string string)
      (slots (list-set slot) :var)
      (dynamic-properties (list-set dynamic-property) :var))
    (deftype instance-opt (union null instance))
    
    (defrecord dynamic-property 
      (name string)
      (value object))
    
    (%heading (4 :semantics) "Slots")
    (defrecord slot
      (id slot-id)
      (value object :var))
    
    (defrecord slot-id (type class))
    
    
    (%heading (2 :semantics) "Qualified Names")
    (deftuple qualified-name (namespace namespace) (name string))
    
    (deftuple partial-name (namespaces (list-set namespace)) (name string))
    
    
    (%heading (2 :semantics) "References")
    (deftuple dot-reference
      (base object)
      (super class-opt)
      (prop-name partial-name))
    
    (deftuple bracket-reference
      (base object)
      (super class-opt)
      (args argument-list))
    
    (deftype reference (union dot-reference bracket-reference))
    (deftype obj-or-ref (union object reference))
    
    
    (%heading (2 :semantics) "Signatures")
    (deftuple signature
      (required-positional (vector class))
      (optional-positional (vector class))
      (optional-named (list-set named-parameter))
      (rest class-opt)
      (rest-allows-names boolean)
      (return-type class))
    
    (deftuple named-parameter
      (name string)
      (type class))
    
    
    (%heading (2 :semantics) "Argument Lists")
    (deftuple named-argument (name string) (value object))
    
    (deftuple argument-list
      (positional (vector object))
      (named (list-set named-argument)))
    
    (%text :comment "The first " (:type object) " is the " (:character-literal "this") " value.")
    (deftype invoker (-> (object argument-list) object))
    
    
    (%heading (2 :semantics) "Unary Operators")
    (deftuple unary-method 
      (operand-type class)
      (f (-> (object object argument-list) object)))
    
    
    (%heading (2 :semantics) "Binary Operators")
    (deftuple binary-method
      (left-type class)
      (right-type class)
      (f (-> (object object) object)))
    
    
    (%heading (1 :semantics) "Data Operations")
    (%heading (2 :semantics) "Numeric Utilities")
    
    (define (u-int32-to-int32 (i integer)) integer
      (if (< i (expt 2 31))
        (return i)
        (return (- i (expt 2 32)))))
    
    (define (to-u-int32 (x float64)) integer
      (rwhen (in x (tag +infinity -infinity nan) :narrow-false)
        (return 0))
      (return (mod (truncate-finite-float64 x) (expt 2 32))))
    
    (define (to-int32 (x float64)) integer
      (return (u-int32-to-int32 (to-u-int32 x))))
    
    
    (%heading (2 :semantics) "Object Utilities")
    (%heading (3 :semantics) (:global object-type nil))
    (%text :comment (:global-call object-type o) " returns an " (:type object) " " (:local o) :apostrophe "s most specific type.")
    (define (object-type (o object)) class
      (case o
        (:select undefined (return undefined-class))
        (:select null (return null-class))
        (:select boolean (return boolean-class))
        (:select float64 (return number-class))
        (:narrow string
          (rwhen (= (length o) 1)
            (return character-class))
          (return string-class))
        (:select namespace (return namespace-class))
        (:select attribute (return attribute-class))
        (:select class (return class-class))
        (:select method-closure (return function-class))
        (:select prototype (return prototype-class))
        (:narrow instance (return (& type o)))))
    
    (%heading (3 :semantics) (:global has-type nil))
    (%text :comment "There are two tests for determining whether an object " (:local o) " is an instance of class " (:local c)
           ". The first, " (:global has-type) ", is used for the purposes of method dispatch and helps determine whether a method of "
           (:local c) " can be called on " (:local o) ". The second, " (:global relaxed-has-type) ", determines whether "
           (:local o) " can be stored in a variable of type " (:local c) " without conversion.")
    
    (%text :comment (:global-call has-type o c) " returns " (:tag true) " if " (:local o) " is an instance of class " (:local c)
           " (or one of " (:local c) :apostrophe "s subclasses). It considers "
           (:tag null) " to be an instance of the classes " (:character-literal "Null") " and " (:character-literal "Object") " only.")
    (define (has-type (o object) (c class)) boolean
      (return (is-ancestor c (object-type o))))
    
    (%text :comment (:global-call relaxed-has-type o c) " returns " (:tag true) " if " (:local o) " is an instance of class " (:local c)
           " (or one of " (:local c) :apostrophe "s subclasses) but considers "
           (:tag null) " to be an instance of the classes " (:character-literal "Null") ", "
           (:character-literal "Object") ", and all other non-primitive classes.")
    (define (relaxed-has-type (o object) (c class)) boolean
      (const t class (object-type o))
      (return (or (is-ancestor c t)
                  (and (= o null object) (not (& primitive c))))))
    
    (%heading (3 :semantics) (:global to-boolean nil))
    (%text :comment (:global-call to-boolean o) " coerces an object " (:local o) " to a Boolean.")
    (define (to-boolean (o object)) boolean
      (case o
        (:select (union undefined null) (return false))
        (:narrow boolean (return o))
        (:narrow float64 (return (not-in o (tag +zero -zero nan))))
        (:narrow string (return (/= o "" string)))
        (:select (union namespace attribute class method-closure prototype) (return true))
        (:select instance (todo))))
    
    (%heading (3 :semantics) (:global to-number nil))
    (%text :comment (:global-call to-number o) " coerces an object " (:local o) " to a number.")
    (define (to-number (o object)) float64
      (case o
        (:select undefined (return nan))
        (:select (union null (tag false)) (return +zero))
        (:select (tag true) (return 1.0))
        (:narrow float64 (return o))
        (:select string (todo))
        (:select (union namespace attribute class method-closure) (throw type-error))
        (:select (union prototype instance) (todo))))
    
    (%heading (3 :semantics) (:global to-string nil))
    (%text :comment (:global-call to-string o) " coerces an object " (:local o) " to a string.")
    (define (to-string (o object)) string
      (case o
        (:select undefined (return "undefined"))
        (:select null (return "null"))
        (:select (tag false) (return "false"))
        (:select (tag true) (return "true"))
        (:select float64 (todo))
        (:narrow string (return o))
        (:select namespace (todo))
        (:select attribute (todo))
        (:select class (todo))
        (:select method-closure (todo))
        (:select (union prototype instance) (todo))))
    
    (%heading (3 :semantics) (:global to-primitive nil))
    (define (to-primitive (o object) (hint object :unused)) object
      (case o
        (:select (union undefined null boolean float64 string) (return o))
        (:select (union namespace attribute class method-closure prototype instance) (return (to-string o)))))
    
    
    (%heading (3 :semantics) (:global unary-plus nil))
    (%text :comment (:global-call unary-plus o) " returns the value of the unary expression " (:character-literal "+") (:local o) ".")
    (define (unary-plus (a object)) object
      (return (unary-dispatch plus-table null null a (new argument-list (vector-of object) (list-set-of named-argument)))))
    
    (%heading (3 :semantics) (:global unary-not nil))
    (%text :comment (:global-call unary-not o) " returns the value of the unary expression " (:character-literal "!") (:local o) ".")
    (define (unary-not (a object)) object
      (return (not (to-boolean a))))
    
    
    (%heading (2 :semantics) "References")
    
    (%text :comment "Read the " (:type obj-or-ref) " " (:local r) ".")
    (define (read-reference (r obj-or-ref)) object
      (case r
        (:narrow object (return r))
        (:narrow dot-reference (return (read-property (& base r) (& prop-name r) (& super r))))
        (:narrow bracket-reference (return (unary-dispatch bracket-read-table (& super r) null (& base r) (& args r))))))
    
    (%text :comment "Write " (:local o) " into the " (:type obj-or-ref) " " (:local r) ".")
    (define (write-reference (r obj-or-ref) (o object)) void
      (case r
        (:select object (throw reference-error))
        (:narrow dot-reference (write-property (& base r) (& prop-name r) (& super r) o))
        (:narrow bracket-reference
          (const args argument-list (new argument-list (append (vector o) (& positional (& args r))) (& named (& args r))))
          (exec (unary-dispatch bracket-write-table (& super r) null (& base r) args)))))
    
    (define (delete-reference (r obj-or-ref)) object
      (case r
        (:select object (throw reference-error))
        (:narrow dot-reference (return (delete-property (& base r) (& prop-name r) (& super r))))
        (:narrow bracket-reference (return (unary-dispatch bracket-delete-table (& super r) null (& base r) (& args r))))))
    
    (define (reference-base (r obj-or-ref)) object
      (case r
        (:narrow object (return null))
        (:narrow reference (return (& base r)))))
    
    
    (%heading (2 :semantics) "Slots")
    
    (define (find-slot (o object) (id slot-id)) slot
      (rwhen (not-in o instance :narrow-false)
        (bottom))
      (const matching-slots (list-set slot)
        (map (& slots o) s s (= (& id s) id slot-id)))
      (assert (= (length matching-slots) 1))
      (return (elt-of matching-slots)))
    
    
    (%heading (2 :semantics) "Member Lookup")
    (%heading (3 :semantics) "Reading a Qualified Property")
    
    (define (read-qualified-property (o object) (name string) (ns namespace) (indexable-only boolean)) object
      (reserve p)
      (rwhen (and (in o (union prototype instance) :narrow-true)
                  (= ns public-namespace namespace)
                  (some (& dynamic-properties o) p (= name (& name p) string) :define-true))
        (return (& value p)))
      (rwhen (and (in o prototype :narrow-true)
                  (not-in (& parent o) (tag null)))
        (return (read-qualified-property (& parent o) name ns indexable-only)))
      (const d member-data-opt (if (in o class :narrow-true)
                                 (most-specific-member o true name ns indexable-only)
                                 (most-specific-member (object-type o) false name ns indexable-only)))
      (case d
        (:select (tag null)
          (rwhen (= (& class-mod (object-type o)) dynamic class-modifier)
            (return undefined))
          (throw property-not-found-error))
        (:narrow global-slot (return (& value d)))
        (:narrow slot-id (return (& value (find-slot o d))))
        (:narrow method
          (return (new method-closure o d)))
        (:narrow accessor
          (return ((& call (& f d)) o (new argument-list (vector-of object) (list-set-of named-argument)))))))
    
    
    (define (most-specific-member (c class) (global boolean) (name string) (ns namespace) (indexable-only boolean)) member-data-opt
      (function (test (m member)) boolean
        (return (and (& readable m)
                     (= name (& name m) string)
                     (set-in ns (& namespaces m))
                     (or (not indexable-only) (& indexable m)))))
      (var ns2 namespace ns)
      (const members (list-set member) (if global (& global-members c) (& instance-members c)))
      (const matches (list-set member) (map members m m (test m)))
      (when (nonempty matches)
        (assert (= (length matches) 1))
        (const d (union member-data namespace) (& data (elt-of matches)))
        (rwhen (not-in d namespace :narrow-both)
          (return d))
        (<- ns2 d))
      (const s class-opt (& super c))
      (rwhen (not-in s (tag null) :narrow-true)
        (return (most-specific-member s global name ns2 indexable-only)))
      (return null))
    
    
    (define (resolve-member-namespace (c class) (global boolean) (name string) (uses (list-set namespace))) namespace-opt
      (const s class-opt (& super c))
      (when (not-in s (tag null) :narrow-true)
        (const ns namespace-opt (resolve-member-namespace s global name uses))
        (rwhen (not-in ns (tag null) :narrow-true)
          (return ns)))
      (function (test (m member)) boolean
        (return (and (& readable m)
                     (= name (& name m) string)
                     (nonempty (set* uses (& namespaces m))))))
      (const members (list-set member) (if global (& global-members c) (& instance-members c)))
      (const matches (list-set member) (map members m m (test m)))
      (rwhen (nonempty matches)
        (rwhen (> (length matches) 1)
          (throw property-not-found-error))
        (const matching-namespaces (list-set namespace) (set* uses (& namespaces (elt-of matches))))
        (return (elt-of matching-namespaces)))
      (return null))
    
    (define (resolve-object-namespace (o object) (name string) (uses (list-set namespace))) namespace
      (const ns namespace-opt (if (in o class :narrow-true)
                                (resolve-member-namespace o true name uses)
                                (resolve-member-namespace (object-type o) false name uses)))
      (rwhen (not-in ns (tag null) :narrow-true)
        (return ns))
      (return public-namespace))
    
    ;Add indexable-only qualifier here.  It can only be used when the only namespace given is public.
    ;Remove read-qualified-property because it's redundant.
    (define (read-property (o object :unused) (prop-name partial-name :unused) (limit class-opt :unused)) object
      (todo))
    ;(const ns namespace (resolve-object-namespace o name uses))
    ;(return (read-qualified-property o name ns false)))
    
    (define (write-property (o object :unused) (prop-name partial-name :unused) (limit class-opt :unused) (new-value object :unused)) void
      (todo))
    
    (define (delete-property (o object :unused) (prop-name partial-name :unused) (limit class-opt :unused)) boolean
      (todo))
    
    (define (write-qualified-property (o object :unused) (name string :unused) (ns namespace :unused) (indexable-only boolean :unused) (new-value object :unused)) void
      (todo))
    
    (define (delete-qualified-property (o object :unused) (name string :unused) (ns namespace :unused) (indexable-only boolean :unused)) boolean
      (todo))
    
    
    (%heading (2 :semantics) "Operator Dispatch")
    (%heading (3 :semantics) "Unary Operators")
    (%text :comment (:global-call unary-dispatch table limit this operand args) " dispatches the unary operator described by " (:local table)
           " applied to the " (:character-literal "this") " value " (:local this) ", the first operand " (:local operand)
           ", and zero or more additional positional and/or named arguments " (:local args)
           ". If " (:local limit) " is non-" (:tag null)
           ", lookup is restricted to operators defined on the proper ancestors of " (:local limit) ".")
    (define (unary-dispatch (table (list-set unary-method)) (limit class-opt) (this object) (operand object) (args argument-list)) object
      (const applicable-ops (list-set unary-method)
        (map table m m (limited-has-type operand (& operand-type m) limit)))
      (reserve best)
      (if (some applicable-ops best
                (every applicable-ops m2 (is-ancestor (& operand-type m2) (& operand-type best))) :define-true)
        (return ((& f best) this operand args))
        (throw property-not-found-error)))
    
    
    (%text :comment (:global-call limited-has-type o c limit) " returns " (:tag true) " if " (:local o) " is a member of class " (:local c)
           " with the added condition that, if " (:local limit) " is non-" (:tag null) ", " (:local c) " is a proper ancestor of " (:local limit) ".")
    (define (limited-has-type (o object) (c class) (limit class-opt)) boolean
      (if (has-type o c)
        (if (in limit (tag null) :narrow-false)
          (return true)
          (return (is-proper-ancestor c limit)))
        (return false)))
    
    
    (%heading (3 :semantics) "Binary Operators")
    (%text :comment (:global-call is-binary-descendant m1 m2) " is " (:tag true) " if " (:local m1) " is at least as specific as " (:local m2)
           " as defined by the procedure below.")
    (define (is-binary-descendant (m1 binary-method) (m2 binary-method)) boolean
      (return (and (is-ancestor (& left-type m2) (& left-type m1))
                   (is-ancestor (& right-type m2) (& right-type m1)))))
    
    (%text :comment (:global-call binary-dispatch table left-limit right-limit left right) " dispatch the binary operator described by " (:local table)
           " applied to the operands " (:local left) " and " (:local right) ". If " (:local left-limit) " is non-" (:tag null)
           ", the lookup is restricted to operator definitions with an ancestor of " (:local left-limit)
           " for the left operand. Similarly, if " (:local right-limit) " is non-" (:tag null)
           ", the lookup is restricted to operator definitions with an ancestor of " (:local right-limit) " for the right operand.")
    (define (binary-dispatch (table (list-set binary-method)) (left-limit class-opt) (right-limit class-opt) (left object) (right object)) object
      (const applicable-ops (list-set binary-method)
        (map table m m (and (limited-has-type left (& left-type m) left-limit)
                            (limited-has-type right (& right-type m) right-limit))))
      (reserve best)
      (if (some applicable-ops best
                (every applicable-ops m2 (is-binary-descendant best m2)) :define-true)
        (return ((& f best) left right))
        (throw property-not-found-error)))
    
    
    (%heading (2 :semantics) "Validation Environments")
    (deftuple validation-env
      (enclosing-class class-opt)
      (labels (vector string))
      (can-return boolean)
      (constants (vector definition)))
    
    (define initial-validation-env validation-env (new validation-env null (vector-of string) false (vector-of definition)))
    
    (%text :comment "Return a " (:type validation-env) " with label " (:local label) " prepended to " (:local v) ".")
    (define (add-label (v validation-env) (label string)) validation-env
      (return (new validation-env (& enclosing-class v) (append (vector label) (& labels v)) (& can-return v) (& constants v))))
    
    (%text :comment "Return " (:tag true) " if this code is inside a class body.")
    (define (inside-class (v validation-env)) boolean
      (return (/= (& enclosing-class v) null class-opt)))
    
    
    (%heading (2 :semantics) "Dynamic Environments")
    (defrecord dynamic-env
      (parent dynamic-env-opt)
      (enclosing-class class-opt)
      (reader-definitions (vector definition) :var)
      (reader-passthroughs (vector qualified-name) :var)
      (writer-definitions (vector definition) :var)
      (writer-passthroughs (vector qualified-name) :var))
    (deftype dynamic-env-opt (union null dynamic-env))
    
    (%text :comment "If the " (:type dynamic-env) " is from within a class" :apostrophe "s body, return that class; otherwise, throw an exception.")
    (define (lexical-class (e dynamic-env :unused)) class
      (todo))
    
    (define (dynamic-env-uses (e dynamic-env :unused)) (list-set namespace)
      (todo))
    
    (define initial-dynamic-env dynamic-env (new dynamic-env null null
                                                 (vector-of definition) (vector-of qualified-name)
                                                 (vector-of definition) (vector-of qualified-name)))
    
    
    (deftuple definition 
      (name qualified-name)
      (type class)
      (data (union slot object accessor)))
    
    
    (define (lookup-variable (e dynamic-env :unused) (name string :unused) (internal-is-namespace boolean :unused)) obj-or-ref
      (todo))
    
    (define (lookup-qualified-variable (e dynamic-env :unused) (namespace namespace :unused) (name string :unused)) obj-or-ref
      (todo))
    
    (%text :comment "Return the value of " (:character-literal "this") ". Throw an exception if there is no " (:character-literal "this") " defined.")
    (define (lookup-this (e dynamic-env :unused)) object
      (todo))
    
    
    (%heading 1 "Expressions")
    (grammar-argument :beta allow-in no-in)
    
    
    (%heading (2 :semantics) "Terminal Actions")
    
    (declare-action name $identifier string :action nil
      (terminal-action name $identifier identity))
    (declare-action eval $number float64 :action nil
      (terminal-action eval $number identity))
    (declare-action eval $string string :action nil
      (terminal-action eval $string identity))
    (%print-actions)
    
    
    (%heading 2 "Identifiers")
    (rule :identifier ((name string))
      (production :identifier ($identifier) identifier-identifier (name (name $identifier)))
      (production :identifier (get) identifier-get (name "get"))
      (production :identifier (set) identifier-set (name "set"))
      (production :identifier (exclude) identifier-exclude (name "exclude"))
      (production :identifier (include) identifier-include (name "include"))
      (production :identifier (named) identifier-named (name "named")))
    (%print-actions)
    
    (%heading 2 "Qualified Identifiers")
    (rule :qualifier ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) namespace)))
      (production :qualifier (:identifier) qualifier-identifier
        ((validate (v :unused)) (todo))
        ((eval e)
         (const a object (read-reference (lookup-variable e (name :identifier) true)))
         (rwhen (not-in a namespace :narrow-false) (throw type-error))
         (return a)))
      (production :qualifier (public) qualifier-public
        ((validate (v :unused)))
        ((eval (e :unused)) (return public-namespace)))
      (production :qualifier (private) qualifier-private
        ((validate v)
         (rwhen (not (inside-class v))
           (throw syntax-error)))
        ((eval e)
         (const q class-opt (& enclosing-class e))
         (rwhen (in q null :narrow-false) (bottom))
         (return (& private-namespace q)))))
    
    (rule :simple-qualified-identifier ((validate (-> (validation-env) void)) (name (-> (dynamic-env) partial-name)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier
        ((validate (v :unused)))
        ((name e) (return (new partial-name (dynamic-env-uses e) (name :identifier))))
        ((eval e) (return (lookup-variable e (name :identifier) false))))
      (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier
        ((validate v) ((validate :qualifier) v))
        ((name e)
         (const q namespace ((eval :qualifier) e))
         (return (new partial-name (list-set q) (name :identifier))))
        ((eval e)
         (const q namespace ((eval :qualifier) e))
         (return (lookup-qualified-variable e q (name :identifier))))))
    
    (rule :expression-qualified-identifier ((validate (-> (validation-env) void)) (name (-> (dynamic-env) partial-name)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :expression-qualified-identifier (:paren-expression \:\: :identifier) expression-qualified-identifier-identifier
        ((validate v)
         ((validate :paren-expression) v)
         (todo))
        ((name e)
         (const q object (read-reference ((eval :paren-expression) e)))
         (rwhen (not-in q namespace :narrow-false) (throw type-error))
         (return (new partial-name (list-set q) (name :identifier))))
        ((eval e)
         (const q object (read-reference ((eval :paren-expression) e)))
         (rwhen (not-in q namespace :narrow-false) (throw type-error))
         (return (lookup-qualified-variable e q (name :identifier))))))
    
    (rule :qualified-identifier ((validate (-> (validation-env) void)) (name (-> (dynamic-env) partial-name)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple
        (validate (validate :simple-qualified-identifier))
        (name (name :simple-qualified-identifier))
        (eval (eval :simple-qualified-identifier)))
      (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression
        (validate (validate :expression-qualified-identifier))
        (name (name :expression-qualified-identifier))
        (eval (eval :expression-qualified-identifier))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Unit Expressions")
    (rule :unit-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :unit-expression (:paren-list-expression) unit-expression-paren-list-expression
        ((validate v) ((validate :paren-list-expression) v))
        ((eval e) (return ((eval :paren-list-expression) e))))
      (production :unit-expression ($number :no-line-break $string) unit-expression-number-with-unit
        ((validate (v :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :unit-expression (:unit-expression :no-line-break $string) unit-expression-unit-expression-with-unit
        ((validate (v :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (%heading 2 "Primary Expressions")
    (rule :primary-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :primary-expression (null) primary-expression-null
        ((validate (v :unused)))
        ((eval (e :unused)) (return null)))
      (production :primary-expression (true) primary-expression-true
        ((validate (v :unused)))
        ((eval (e :unused)) (return true)))
      (production :primary-expression (false) primary-expression-false
        ((validate (v :unused)))
        ((eval (e :unused)) (return false)))
      (production :primary-expression (public) primary-expression-public
        ((validate (v :unused)))
        ((eval (e :unused)) (return public-namespace)))
      (production :primary-expression ($number) primary-expression-number
        ((validate (v :unused)))
        ((eval (e :unused)) (return (eval $number))))
      (production :primary-expression ($string) primary-expression-string
        ((validate (v :unused)))
        ((eval (e :unused)) (return (eval $string))))
      (production :primary-expression (this) primary-expression-this
        ((validate (v :unused)) (todo))
        ((eval e) (return (lookup-this e))))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((validate (v :unused)))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:unit-expression) primary-expression-unit-expression
        ((validate v) ((validate :unit-expression) v))
        ((eval e) (return ((eval :unit-expression) e))))
      (production :primary-expression (:array-literal) primary-expression-array-literal
        ((validate (v :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:object-literal) primary-expression-object-literal
        ((validate (v :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:function-expression) primary-expression-function-expression
        ((validate v) ((validate :function-expression) v))
        ((eval e) (return ((eval :function-expression) e)))))
    
    (rule :paren-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :paren-expression (\( (:assignment-expression allow-in) \)) paren-expression-assignment-expression
        (validate (validate :assignment-expression))
        (eval (eval :assignment-expression))))
    
    (rule :paren-list-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (eval-as-list (-> (dynamic-env) (vector object))))
      (production :paren-list-expression (:paren-expression) paren-list-expression-paren-expression
        ((validate v) ((validate :paren-expression) v))
        ((eval e) (return ((eval :paren-expression) e)))
        ((eval-as-list e)
         (const elt object (read-reference ((eval :paren-expression) e)))
         (return (vector elt))))
      (production :paren-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) paren-list-expression-list-expression
        ((validate v)
         ((validate :list-expression) v)
         ((validate :assignment-expression) v))
        ((eval e)
         (exec (read-reference ((eval :list-expression) e)))
         (return (read-reference ((eval :assignment-expression) e))))
        ((eval-as-list e)
         (const elts (vector object) ((eval-as-list :list-expression) e))
         (const elt object (read-reference ((eval :assignment-expression) e)))
         (return (append elts (vector elt))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Function Expressions")
    (rule :function-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :function-expression (function :function-signature :block) function-expression-anonymous
        ((validate (v :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :function-expression (function :identifier :function-signature :block) function-expression-named
        ((validate (v :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Object Literals")
    (production :object-literal (\{ \}) object-literal-empty)
    (production :object-literal (\{ :field-list \}) object-literal-list)
    
    (production :field-list (:literal-field) field-list-one)
    (production :field-list (:field-list \, :literal-field) field-list-more)
    
    (rule :literal-field ((validate (-> (validation-env) (list-set string))) (eval (-> (dynamic-env) named-argument)))
      (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression
        ((validate v)
         (const names (list-set string) ((validate :field-name) v))
         ((validate :assignment-expression) v)
         (return names))
        ((eval e)
         (const name string ((eval :field-name) e))
         (const value object (read-reference ((eval :assignment-expression) e)))
         (return (new named-argument name value)))))
    
    (rule :field-name ((validate (-> (validation-env) (list-set string))) (eval (-> (dynamic-env) string)))
      (production :field-name (:identifier) field-name-identifier
        ((validate (v :unused)) (return (list-set (name :identifier))))
        ((eval (e :unused)) (return (name :identifier))))
      (production :field-name ($string) field-name-string
        ((validate (v :unused)) (return (list-set (eval $string))))
        ((eval (e :unused)) (return (eval $string))))
      (production :field-name ($number) field-name-number
        ((validate (v :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (? js2
        (production :field-name (:paren-expression) field-name-paren-expression
          ((validate (v :unused)) (todo))
          ((eval (e :unused)) (todo)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Super Expressions")
    (rule :super-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) object)) (super (-> (dynamic-env) class)))
      (production :super-expression (super) super-expression-super
        ((validate v)
         (rwhen (not (inside-class v))
           (throw syntax-error)))
        ((eval e) (return (lookup-this e)))
        ((super e) (return (lexical-class e))))
      (production :super-expression (:full-super-expression) super-expression-full-super-expression
        ((validate v) ((validate :full-super-expression) v))
        ((eval e) (return ((eval :full-super-expression) e)))
        ((super e) (return ((super :full-super-expression) e)))))
    
    (rule :full-super-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) object)) (super (-> (dynamic-env) class)))
      (production :full-super-expression (super :paren-expression) full-super-expression-super-paren-expression
        ((validate v)
         (rwhen (not (inside-class v))
           (throw syntax-error))
         ((validate :paren-expression) v))
        ((eval e)
         (const a object (read-reference ((eval :paren-expression) e)))
         (const c class (lexical-class e))
         (rwhen (not (has-type a c))
           (throw type-error))
         (return a))
        ((super e) (return (lexical-class e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Postfix Expressions")
    (rule :postfix-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        (validate (validate :attribute-expression))
        (eval (eval :attribute-expression)))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        (validate (validate :full-postfix-expression))
        (eval (eval :full-postfix-expression)))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        (validate (validate :short-new-expression))
        (eval (eval :short-new-expression))))
    
    (rule :postfix-expression-or-super ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production :postfix-expression-or-super (:postfix-expression) postfix-expression-or-super-postfix-expression
        (validate (validate :postfix-expression))
        (eval (eval :postfix-expression))
        ((super (e :unused)) (return null)))
      (production :postfix-expression-or-super (:super-expression) postfix-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    
    (rule :attribute-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier
        ((validate v) ((validate :simple-qualified-identifier) v))
        ((eval e) (return ((eval :simple-qualified-identifier) e))))
      (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator
        ((validate v)
         ((validate :attribute-expression) v)
         ((validate :member-operator) v))
        ((eval e)
         (const a object (read-reference ((eval :attribute-expression) e)))
         (return ((eval :member-operator) e a))))
      (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call
        ((validate v)
         ((validate :attribute-expression) v)
         ((validate :arguments) v))
        ((eval e)
         (const r obj-or-ref ((eval :attribute-expression) e))
         (const f object (read-reference r))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch call-table null base f args)))))
    
    (rule :full-postfix-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression
        ((validate v) ((validate :primary-expression) v))
        ((eval e) (return ((eval :primary-expression) e))))
      (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier
        ((validate v) ((validate :expression-qualified-identifier) v))
        ((eval e) (return ((eval :expression-qualified-identifier) e))))
      (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression
        ((validate v) ((validate :full-new-expression) v))
        ((eval e) (return ((eval :full-new-expression) e))))
      (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator
        ((validate v)
         ((validate :full-postfix-expression) v)
         ((validate :member-operator) v))
        ((eval e)
         (const a object (read-reference ((eval :full-postfix-expression) e)))
         (return ((eval :member-operator) e a))))
      (production :full-postfix-expression (:super-expression :dot-operator) full-postfix-expression-super-dot-operator
        ((validate v)
         ((validate :super-expression) v)
         ((validate :dot-operator) v))
        ((eval e)
         (const a object ((eval :super-expression) e))
         (const sa class ((super :super-expression) e))
         (return ((eval :dot-operator) e a sa))))
      (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call
        ((validate v)
         ((validate :full-postfix-expression) v)
         ((validate :arguments) v))
        ((eval e)
         (const r obj-or-ref ((eval :full-postfix-expression) e))
         (const f object (read-reference r))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch call-table null base f args))))
      (production :full-postfix-expression (:full-super-expression :arguments) full-postfix-expression-super-call
        ((validate v)
         ((validate :full-super-expression) v)
         ((validate :arguments) v))
        ((eval e)
         (const f object ((eval :full-super-expression) e))
         (const sf class ((super :full-super-expression) e))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch call-table sf null f args))))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break ++) full-postfix-expression-increment
        ((validate v) ((validate :postfix-expression-or-super) v))
        ((eval e)
         (const r obj-or-ref ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch increment-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return a)))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break --) full-postfix-expression-decrement
        ((validate v) ((validate :postfix-expression-or-super) v))
        ((eval e)
         (const r obj-or-ref ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch decrement-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return a))))
    
    (rule :full-new-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new
        ((validate v)
         ((validate :full-new-subexpression) v)
         ((validate :arguments) v))
        ((eval e)
         (const f object (read-reference ((eval :full-new-subexpression) e)))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch construct-table null null f args))))
      (production :full-new-expression (new :full-super-expression :arguments) full-new-expression-super-new
        ((validate v)
         ((validate :full-super-expression) v)
         ((validate :arguments) v))
        ((eval e)
         (const f object ((eval :full-super-expression) e))
         (const sf class ((super :full-super-expression) e))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch construct-table sf null f args)))))
    
    (rule :full-new-subexpression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression
        ((validate v) ((validate :primary-expression) v))
        ((eval e) (return ((eval :primary-expression) e))))
      (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier
        ((validate v) ((validate :qualified-identifier) v))
        ((eval e) (return ((eval :qualified-identifier) e))))
      (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression
        ((validate v) ((validate :full-new-expression) v))
        ((eval e) (return ((eval :full-new-expression) e))))
      (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator
        ((validate v)
         ((validate :full-new-subexpression) v)
         ((validate :member-operator) v))
        ((eval e)
         (const a object (read-reference ((eval :full-new-subexpression) e)))
         (return ((eval :member-operator) e a))))
      (production :full-new-subexpression (:super-expression :dot-operator) full-new-subexpression-super-dot-operator
        ((validate v)
         ((validate :super-expression) v)
         ((validate :dot-operator) v))
        ((eval e)
         (const a object ((eval :super-expression) e))
         (const sa class ((super :super-expression) e))
         (return ((eval :dot-operator) e a sa)))))
    
    (rule :short-new-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :short-new-expression (new :short-new-subexpression) short-new-expression-new
        ((validate v) ((validate :short-new-subexpression) v))
        ((eval e)
         (const f object (read-reference ((eval :short-new-subexpression) e)))
         (return (unary-dispatch construct-table null null f (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :short-new-expression (new :super-expression) short-new-expression-super-new
        ((validate v) ((validate :super-expression) v))
        ((eval e)
         (const f object ((eval :super-expression) e))
         (const sf class ((super :super-expression) e))
         (return (unary-dispatch construct-table sf null f (new argument-list (vector-of object) (list-set-of named-argument)))))))
    
    (rule :short-new-subexpression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full
        (validate (validate :full-new-subexpression))
        (eval (eval :full-new-subexpression)))
      (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short
        (validate (validate :short-new-expression))
        (eval (eval :short-new-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Member Operators")
    (rule :member-operator ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) obj-or-ref)))
      (production :member-operator (:dot-operator) member-operator-dot-operator
        ((validate v) ((validate :dot-operator) v))
        ((eval e a) (return ((eval :dot-operator) e a null))))
      (production :member-operator (\. :paren-expression) member-operator-indirect
        ((validate v) ((validate :paren-expression) v))
        ((eval (e :unused) (a :unused)) (todo))))
    
    (rule :dot-operator ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object class-opt) obj-or-ref)))
      (production :dot-operator (\. :qualified-identifier) dot-operator-qualified-identifier
        ((validate v) ((validate :qualified-identifier) v))
        ((eval e a sa)
         (const n partial-name ((name :qualified-identifier) e))
         (return (new dot-reference a sa n))))
      (production :dot-operator (:brackets) dot-operator-brackets
        ((validate v) ((validate :brackets) v))
        ((eval e a sa)
         (const args argument-list ((eval :brackets) e))
         (return (new bracket-reference a sa args)))))
    
    (rule :brackets ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) argument-list)))
      (production :brackets ([ ]) brackets-none
        ((validate (v :unused)))
        ((eval (e :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed
        ((validate v) ((validate :list-expression) v))
        ((eval e)
         (const positional (vector object) ((eval-as-list :list-expression) e))
         (return (new argument-list positional (list-set-of named-argument)))))
      (production :brackets ([ :named-argument-list ]) brackets-named
        ((validate v) (exec ((validate :named-argument-list) v)))
        ((eval e) (return ((eval :named-argument-list) e)))))
    
    (rule :arguments ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) argument-list)))
      (production :arguments (:paren-expressions) arguments-paren-expressions
        ((validate v) ((validate :paren-expressions) v))
        ((eval e) (return ((eval :paren-expressions) e))))
      (production :arguments (\( :named-argument-list \)) arguments-named
        ((validate v) (exec ((validate :named-argument-list) v)))
        ((eval e) (return ((eval :named-argument-list) e)))))
    
    (rule :paren-expressions ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) argument-list)))
      (production :paren-expressions (\( \)) paren-expressions-none
        ((validate (v :unused)))
        ((eval (e :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :paren-expressions (:paren-list-expression) paren-expressions-some
        ((validate v) ((validate :paren-list-expression) v))
        ((eval e)
         (const positional (vector object) ((eval-as-list :paren-list-expression) e))
         (return (new argument-list positional (list-set-of named-argument))))))
    
    (rule :named-argument-list ((validate (-> (validation-env) (list-set string))) (eval (-> (dynamic-env) argument-list)))
      (production :named-argument-list (:literal-field) named-argument-list-one
        ((validate v) (return ((validate :literal-field) v)))
        ((eval e)
         (const na named-argument ((eval :literal-field) e))
         (return (new argument-list (vector-of object) (list-set na)))))
      (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed
        ((validate v)
         ((validate :list-expression) v)
         (return ((validate :literal-field) v)))
        ((eval e)
         (const positional (vector object) ((eval-as-list :list-expression) e))
         (const na named-argument ((eval :literal-field) e))
         (return (new argument-list positional (list-set na)))))
      (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more
        ((validate v)
         (const names1 (list-set string) ((validate :named-argument-list) v))
         (const names2 (list-set string) ((validate :literal-field) v))
         (rwhen (nonempty (set* names1 names2))
           (throw syntax-error))
         (return (set+ names1 names2)))
        ((eval e)
         (const args argument-list ((eval :named-argument-list) e))
         (const na named-argument ((eval :literal-field) e))
         (rwhen (some (& named args) na2 (= (& name na2) (& name na) string))
           (throw argument-mismatch-error))
         (return (new argument-list (& positional args) (set+ (& named args) (list-set na)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Unary Operators")
    (rule :unary-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :unary-expression (:postfix-expression) unary-expression-postfix
        ((validate v) ((validate :postfix-expression) v))
        ((eval e) (return ((eval :postfix-expression) e))))
      (production :unary-expression (delete :postfix-expression) unary-expression-delete
        ((validate v) ((validate :postfix-expression) v))
        ((eval e) (return (delete-reference ((eval :postfix-expression) e)))))
      (production :unary-expression (void :unary-expression) unary-expression-void
        ((validate v) ((validate :unary-expression) v))
        ((eval e)
         (exec (read-reference ((eval :unary-expression) e)))
         (return undefined)))
      (production :unary-expression (typeof :unary-expression) unary-expression-typeof
        ((validate v) ((validate :unary-expression) v))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression) e)))
         (case a
           (:select undefined (return "undefined"))
           (:select (union null prototype) (return "object"))
           (:select boolean (return "boolean"))
           (:select float64 (return "number"))
           (:select string (return "string"))
           (:select namespace (return "namespace"))
           (:select attribute (return "attribute"))
           (:select (union class method-closure) (return "function"))
           (:narrow instance (return (& typeof-string a))))))
      (production :unary-expression (++ :postfix-expression-or-super) unary-expression-increment
        ((validate v) ((validate :postfix-expression-or-super) v))
        ((eval e)
         (const r obj-or-ref ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch increment-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return b)))
      (production :unary-expression (-- :postfix-expression-or-super) unary-expression-decrement
        ((validate v) ((validate :postfix-expression-or-super) v))
        ((eval e)
         (const r obj-or-ref ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch decrement-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return b)))
      (production :unary-expression (+ :unary-expression-or-super) unary-expression-plus
        ((validate v) ((validate :unary-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch plus-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (- :unary-expression-or-super) unary-expression-minus
        ((validate v) ((validate :unary-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch minus-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (~ :unary-expression-or-super) unary-expression-bitwise-not
        ((validate v) ((validate :unary-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch bitwise-not-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (! :unary-expression) unary-expression-logical-not
        ((validate v) ((validate :unary-expression) v))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression) e)))
         (return (unary-not a)))))
    
    (rule :unary-expression-or-super ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production :unary-expression-or-super (:unary-expression) unary-expression-or-super-unary-expression
        (validate (validate :unary-expression))
        (eval (eval :unary-expression))
        ((super (e :unused)) (return null)))
      (production :unary-expression-or-super (:super-expression) unary-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Multiplicative Operators")
    (rule :multiplicative-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        ((validate v) ((validate :unary-expression) v))
        ((eval e) (return ((eval :unary-expression) e))))
      (production :multiplicative-expression (:multiplicative-expression-or-super * :unary-expression-or-super) multiplicative-expression-multiply
        ((validate v)
         ((validate :multiplicative-expression-or-super) v)
         ((validate :unary-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch multiply-table sa sb a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super / :unary-expression-or-super) multiplicative-expression-divide
        ((validate v)
         ((validate :multiplicative-expression-or-super) v)
         ((validate :unary-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch divide-table sa sb a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super % :unary-expression-or-super) multiplicative-expression-remainder
        ((validate v)
         ((validate :multiplicative-expression-or-super) v)
         ((validate :unary-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch remainder-table sa sb a b)))))
    
    (rule :multiplicative-expression-or-super ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production :multiplicative-expression-or-super (:multiplicative-expression) multiplicative-expression-or-super-multiplicative-expression
        (validate (validate :multiplicative-expression))
        (eval (eval :multiplicative-expression))
        ((super (e :unused)) (return null)))
      (production :multiplicative-expression-or-super (:super-expression) multiplicative-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Additive Operators")
    (rule :additive-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        ((validate v) ((validate :multiplicative-expression) v))
        ((eval e) (return ((eval :multiplicative-expression) e))))
      (production :additive-expression (:additive-expression-or-super + :multiplicative-expression-or-super) additive-expression-add
        ((validate v)
         ((validate :additive-expression-or-super) v)
         ((validate :multiplicative-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :additive-expression-or-super) e)))
         (const b object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const sa class-opt ((super :additive-expression-or-super) e))
         (const sb class-opt ((super :multiplicative-expression-or-super) e))
         (return (binary-dispatch add-table sa sb a b))))
      (production :additive-expression (:additive-expression-or-super - :multiplicative-expression-or-super) additive-expression-subtract
        ((validate v)
         ((validate :additive-expression-or-super) v)
         ((validate :multiplicative-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :additive-expression-or-super) e)))
         (const b object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const sa class-opt ((super :additive-expression-or-super) e))
         (const sb class-opt ((super :multiplicative-expression-or-super) e))
         (return (binary-dispatch subtract-table sa sb a b)))))
     
    (rule :additive-expression-or-super ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production :additive-expression-or-super (:additive-expression) additive-expression-or-super-additive-expression
        (validate (validate :additive-expression))
        (eval (eval :additive-expression))
        ((super (e :unused)) (return null)))
      (production :additive-expression-or-super (:super-expression) additive-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Bitwise Shift Operators")
    (rule :shift-expression ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        ((validate v) ((validate :additive-expression) v))
        ((eval e) (return ((eval :additive-expression) e))))
      (production :shift-expression (:shift-expression-or-super << :additive-expression-or-super) shift-expression-left
        ((validate v)
         ((validate :shift-expression-or-super) v)
         ((validate :additive-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-left-table sa sb a b))))
      (production :shift-expression (:shift-expression-or-super >> :additive-expression-or-super) shift-expression-right-signed
        ((validate v)
         ((validate :shift-expression-or-super) v)
         ((validate :additive-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-right-table sa sb a b))))
      (production :shift-expression (:shift-expression-or-super >>> :additive-expression-or-super) shift-expression-right-unsigned
        ((validate v)
         ((validate :shift-expression-or-super) v)
         ((validate :additive-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-right-unsigned-table sa sb a b)))))
    
    (rule :shift-expression-or-super ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production :shift-expression-or-super (:shift-expression) shift-expression-or-super-shift-expression
        (validate (validate :shift-expression))
        (eval (eval :shift-expression))
        ((super (e :unused)) (return null)))
      (production :shift-expression-or-super (:super-expression) shift-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Relational Operators")
    (rule (:relational-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
        ((validate v) ((validate :shift-expression) v))
        ((eval e) (return ((eval :shift-expression) e))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) < :shift-expression-or-super) relational-expression-less
        ((validate v)
         ((validate :relational-expression-or-super) v)
         ((validate :shift-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-table sa sb a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) > :shift-expression-or-super) relational-expression-greater
        ((validate v)
         ((validate :relational-expression-or-super) v)
         ((validate :shift-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-table sb sa b a))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) <= :shift-expression-or-super) relational-expression-less-or-equal
        ((validate v)
         ((validate :relational-expression-or-super) v)
         ((validate :shift-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-or-equal-table sa sb a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) >= :shift-expression-or-super) relational-expression-greater-or-equal
        ((validate v)
         ((validate :relational-expression-or-super) v)
         ((validate :shift-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-or-equal-table sb sa b a))))
      (production (:relational-expression :beta) ((:relational-expression :beta) is :shift-expression) relational-expression-is
        ((validate v)
         ((validate :relational-expression) v)
         ((validate :shift-expression) v))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((validate v)
         ((validate :relational-expression) v)
         ((validate :shift-expression) v))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression-or-super) relational-expression-in
        ((validate v)
         ((validate :relational-expression) v)
         ((validate :shift-expression-or-super) v))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((validate v)
         ((validate :relational-expression) v)
         ((validate :shift-expression) v))
        ((eval (e :unused)) (todo))))
    
    (rule (:relational-expression-or-super :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production (:relational-expression-or-super :beta) ((:relational-expression :beta)) relational-expression-or-super-relational-expression
        (validate (validate :relational-expression))
        (eval (eval :relational-expression))
        ((super (e :unused)) (return null)))
      (production (:relational-expression-or-super :beta) (:super-expression) relational-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Equality Operators")
    (rule (:equality-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        ((validate v) ((validate :relational-expression) v))
        ((eval e) (return ((eval :relational-expression) e))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) == (:relational-expression-or-super :beta)) equality-expression-equal
        ((validate v)
         ((validate :equality-expression-or-super) v)
         ((validate :relational-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (binary-dispatch equal-table sa sb a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) != (:relational-expression-or-super :beta)) equality-expression-not-equal
        ((validate v)
         ((validate :equality-expression-or-super) v)
         ((validate :relational-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not (binary-dispatch equal-table sa sb a b)))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) === (:relational-expression-or-super :beta)) equality-expression-strict-equal
        ((validate v)
         ((validate :equality-expression-or-super) v)
         ((validate :relational-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (binary-dispatch strict-equal-table sa sb a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) !== (:relational-expression-or-super :beta)) equality-expression-strict-not-equal
        ((validate v)
         ((validate :equality-expression-or-super) v)
         ((validate :relational-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not (binary-dispatch strict-equal-table sa sb a b))))))
    
    (rule (:equality-expression-or-super :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production (:equality-expression-or-super :beta) ((:equality-expression :beta)) equality-expression-or-super-equality-expression
        (validate (validate :equality-expression))
        (eval (eval :equality-expression))
        ((super (e :unused)) (return null)))
      (production (:equality-expression-or-super :beta) (:super-expression) equality-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        ((validate v) ((validate :equality-expression) v))
        ((eval e) (return ((eval :equality-expression) e))))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression-or-super :beta) & (:equality-expression-or-super :beta)) bitwise-and-expression-and
        ((validate v)
         ((validate :bitwise-and-expression-or-super) v)
         ((validate :equality-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-and-expression-or-super) e)))
         (const b object (read-reference ((eval :equality-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-and-expression-or-super) e))
         (const sb class-opt ((super :equality-expression-or-super) e))
         (return (binary-dispatch bitwise-and-table sa sb a b)))))
    
    (rule (:bitwise-xor-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        ((validate v) ((validate :bitwise-and-expression) v))
        ((eval e) (return ((eval :bitwise-and-expression) e))))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression-or-super :beta) ^ (:bitwise-and-expression-or-super :beta)) bitwise-xor-expression-xor
        ((validate v)
         ((validate :bitwise-xor-expression-or-super) v)
         ((validate :bitwise-and-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-xor-expression-or-super) e)))
         (const b object (read-reference ((eval :bitwise-and-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-xor-expression-or-super) e))
         (const sb class-opt ((super :bitwise-and-expression-or-super) e))
         (return (binary-dispatch bitwise-xor-table sa sb a b)))))
    
    (rule (:bitwise-or-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        ((validate v) ((validate :bitwise-xor-expression) v))
        ((eval e) (return ((eval :bitwise-xor-expression) e))))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression-or-super :beta) \| (:bitwise-xor-expression-or-super :beta)) bitwise-or-expression-or
        ((validate v)
         ((validate :bitwise-or-expression-or-super) v)
         ((validate :bitwise-xor-expression-or-super) v))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-or-expression-or-super) e)))
         (const b object (read-reference ((eval :bitwise-xor-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-or-expression-or-super) e))
         (const sb class-opt ((super :bitwise-xor-expression-or-super) e))
         (return (binary-dispatch bitwise-or-table sa sb a b)))))
    
    
    (rule (:bitwise-and-expression-or-super :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-and-expression-or-super :beta) ((:bitwise-and-expression :beta)) bitwise-and-expression-or-super-bitwise-and-expression
        (validate (validate :bitwise-and-expression))
        (eval (eval :bitwise-and-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-and-expression-or-super :beta) (:super-expression) bitwise-and-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    
    (rule (:bitwise-xor-expression-or-super :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-xor-expression-or-super :beta) ((:bitwise-xor-expression :beta)) bitwise-xor-expression-or-super-bitwise-xor-expression
        (validate (validate :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-xor-expression-or-super :beta) (:super-expression) bitwise-xor-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    
    (rule (:bitwise-or-expression-or-super :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-or-expression-or-super :beta) ((:bitwise-or-expression :beta)) bitwise-or-expression-or-super-bitwise-or-expression
        (validate (validate :bitwise-or-expression))
        (eval (eval :bitwise-or-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-or-expression-or-super :beta) (:super-expression) bitwise-or-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))
        ((super e) (return ((super :super-expression) e)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Binary Logical Operators")
    (rule (:logical-and-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        ((validate v) ((validate :bitwise-or-expression) v))
        ((eval e) (return ((eval :bitwise-or-expression) e))))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((validate v)
         ((validate :logical-and-expression) v)
         ((validate :bitwise-or-expression) v))
        ((eval e)
         (const a object (read-reference ((eval :logical-and-expression) e)))
         (if (to-boolean a)
           (return (read-reference ((eval :bitwise-or-expression) e)))
           (return a)))))
    
    (rule (:logical-xor-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        ((validate v) ((validate :logical-and-expression) v))
        ((eval e) (return ((eval :logical-and-expression) e))))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((validate v)
         ((validate :logical-xor-expression) v)
         ((validate :logical-and-expression) v))
        ((eval e)
         (const a object (read-reference ((eval :logical-xor-expression) e)))
         (const b object (read-reference ((eval :logical-and-expression) e)))
         (const ab boolean (to-boolean a))
         (const bb boolean (to-boolean b))
         (return (xor ab bb)))))
    
    (rule (:logical-or-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        ((validate v) ((validate :logical-xor-expression) v))
        ((eval e) (return ((eval :logical-xor-expression) e))))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((validate v)
         ((validate :logical-or-expression) v)
         ((validate :logical-xor-expression) v))
        ((eval e)
         (const a object (read-reference ((eval :logical-or-expression) e)))
         (if (to-boolean a) 
           (return a)
           (return (read-reference ((eval :logical-xor-expression) e)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Conditional Operator")
    (rule (:conditional-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        ((validate v) ((validate :logical-or-expression) v))
        ((eval e) (return ((eval :logical-or-expression) e))))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((validate v)
         ((validate :logical-or-expression) v)
         ((validate :assignment-expression 1) v)
         ((validate :assignment-expression 2) v))
        ((eval e)
         (if (to-boolean (read-reference ((eval :logical-or-expression) e)))
           (return ((eval :assignment-expression 1) e))
           (return ((eval :assignment-expression 2) e))))))
    
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or)
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Assignment Operators")
    (rule (:assignment-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        ((validate v) ((validate :conditional-expression) v))
        ((eval e) (return ((eval :conditional-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((validate v)
         ((validate :postfix-expression) v)
         ((validate :assignment-expression) v))
        ((eval e)
         (const r obj-or-ref ((eval :postfix-expression) e))
         (const a object (read-reference ((eval :assignment-expression) e)))
         (write-reference r a)
         (return a)))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((validate v)
         ((validate :postfix-expression-or-super) v)
         ((validate :assignment-expression) v))
        ((eval e)
         (return (eval-assignment-op (table :compound-assignment) ((super :postfix-expression-or-super) e) null
                                     (eval :postfix-expression-or-super) (eval :assignment-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment :super-expression) assignment-expression-compound-super
        ((validate v)
         ((validate :postfix-expression-or-super) v)
         ((validate :super-expression) v))
        ((eval e)
         (return (eval-assignment-op (table :compound-assignment) ((super :postfix-expression-or-super) e) ((super :super-expression) e)
                                     (eval :postfix-expression-or-super) (eval :super-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((validate v)
         ((validate :postfix-expression) v)
         ((validate :assignment-expression) v))
        ((eval (e :unused)) (todo))))
    
    (rule :compound-assignment ((table (list-set binary-method)))
      (production :compound-assignment (*=) compound-assignment-multiply (table multiply-table))
      (production :compound-assignment (/=) compound-assignment-divide (table divide-table))
      (production :compound-assignment (%=) compound-assignment-remainder (table remainder-table))
      (production :compound-assignment (+=) compound-assignment-add (table add-table))
      (production :compound-assignment (-=) compound-assignment-subtract (table subtract-table))
      (production :compound-assignment (<<=) compound-assignment-shift-left (table shift-left-table))
      (production :compound-assignment (>>=) compound-assignment-shift-right (table shift-right-table))
      (production :compound-assignment (>>>=) compound-assignment-shift-right-unsigned (table shift-right-unsigned-table))
      (production :compound-assignment (&=) compound-assignment-bitwise-and (table bitwise-and-table))
      (production :compound-assignment (^=) compound-assignment-bitwise-xor (table bitwise-xor-table))
      (production :compound-assignment (\|=) compound-assignment-bitwise-or (table bitwise-or-table)))
    
    (production :logical-assignment (&&=) logical-assignment-logical-and)
    (production :logical-assignment (^^=) logical-assignment-logical-xor)
    (production :logical-assignment (\|\|=) logical-assignment-logical-or)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (eval-assignment-op (table (list-set binary-method)) (left-limit class-opt) (right-limit class-opt)
                                (left-eval (-> (dynamic-env) obj-or-ref)) (right-eval (-> (dynamic-env) obj-or-ref)) (e dynamic-env)) obj-or-ref
      (const r-left obj-or-ref (left-eval e))
      (const o-left object (read-reference r-left))
      (const o-right object (read-reference (right-eval e)))
      (const result object (binary-dispatch table left-limit right-limit o-left o-right))
      (write-reference r-left result)
      (return result))
    
    
    (%heading 2 "Comma Expressions")
    (rule (:list-expression :beta) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) obj-or-ref)) (eval-as-list (-> (dynamic-env) (vector object))))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        ((validate v) ((validate :assignment-expression) v))
        ((eval e) (return ((eval :assignment-expression) e)))
        ((eval-as-list e)
         (const elt object (read-reference ((eval :assignment-expression) e)))
         (return (vector elt))))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((validate v)
         ((validate :list-expression) v)
         ((validate :assignment-expression) v))
        ((eval e)
         (exec (read-reference ((eval :list-expression) e)))
         (return (read-reference ((eval :assignment-expression) e))))
        ((eval-as-list e)
         (const elts (vector object) ((eval-as-list :list-expression) e))
         (const elt object (read-reference ((eval :assignment-expression) e)))
         (return (append elts (vector elt))))))
    
    (production :optional-expression ((:list-expression allow-in)) optional-expression-expression)
    (production :optional-expression () optional-expression-empty)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Type Expressions")
    (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 1 "Statements")
    
    (grammar-argument :omega
                      abbrev       ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)        ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (rule (:statement :omega) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:statement :omega) (:empty-statement) statement-empty-statement
        ((validate (v :unused)))
        ((eval (e :unused) d) (return d)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        ((validate v) ((validate :expression-statement) v))
        ((eval e (d :unused)) (return ((eval :expression-statement) e))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:annotated-block) statement-annotated-block
        ((validate v) ((validate :annotated-block) v))
        ((eval e d) (return ((eval :annotated-block) e d))))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        ((validate v) ((validate :labeled-statement) v))
        ((eval e d) (return ((eval :labeled-statement) e d))))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        ((validate v) ((validate :if-statement) v))
        ((eval e d) (return ((eval :if-statement) e d))))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        ((validate v) ((validate :continue-statement) v))
        ((eval e d) (return ((eval :continue-statement) e d))))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        ((validate v) ((validate :break-statement) v))
        ((eval e d) (return ((eval :break-statement) e d))))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        ((validate v) ((validate :return-statement) v))
        ((eval e (d :unused)) (return ((eval :return-statement) e))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    
    (rule (:substatement :omega) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        ((validate v) ((validate :statement) v))
        ((eval e d) (return ((eval :statement) e d))))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon no-short-if) () semicolon-no-short-if)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%heading 2 "Expression Statement")
    (rule :expression-statement ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        ((validate v) ((validate :list-expression) v))
        ((eval e) (return (read-reference ((eval :list-expression) e))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Super Statement")
    (production :super-statement (super :arguments) super-statement-super-arguments)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Block Statement")
    (rule :annotated-block ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production :annotated-block (:attributes :block) annotated-block-attributes-and-block
        (validate (validate :block)) ;******
        (eval (eval :block)))) ;******
    
    (rule :block ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production :block ({ :directives }) block-directives
        (validate (validate :directives))
        (eval (eval :directives))))
    
    (rule :directives ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production :directives () directives-none
        ((validate (v :unused)))
        ((eval (e :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((validate v)
         ((validate :directives-prefix) v)
         ((validate :directive) v))
        ((eval e d)
         (const o object ((eval :directives-prefix) e d))
         (return ((eval :directive) e o)))))
    
    (rule :directives-prefix ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production :directives-prefix () directives-prefix-none
        ((validate (v :unused)))
        ((eval (e :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((validate v)
         ((validate :directives-prefix) v)
         ((validate :directive) v))
        ((eval e d)
         (const o object ((eval :directives-prefix) e d))
         (return ((eval :directive) e o)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Labeled Statements")
    (rule (:labeled-statement :omega) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((validate v) ((validate :substatement) (add-label v (name :identifier))))
        ((eval e d)
         (catch ((return ((eval :substatement) e d)))
           (x) (if (and (in x go-break :narrow-true) (= (& label x) (name :identifier) string))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "If Statement")
    (rule (:if-statement :omega) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:if-statement abbrev) (if :paren-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((validate v)
         ((validate :paren-list-expression) v)
         ((validate :substatement) v))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) e)))
           (return ((eval :substatement) e d))
           (return d))))
      (production (:if-statement full) (if :paren-list-expression (:substatement full)) if-statement-if-then-full
        ((validate v)
         ((validate :paren-list-expression) v)
         ((validate :substatement) v))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) e)))
           (return ((eval :substatement) e d))
           (return d))))
      (production (:if-statement :omega) (if :paren-list-expression (:substatement no-short-if) else (:substatement :omega))
                  if-statement-if-then-else
        ((validate v)
         ((validate :paren-list-expression) v)
         ((validate :substatement 1) v)
         ((validate :substatement 2) v))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) e)))
           (return ((eval :substatement 1) e d))
           (return ((eval :substatement 2) e d))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Switch Statement")
    (production :switch-statement (switch :paren-list-expression { :case-statements }) switch-statement-cases)
    
    (production :case-statements () case-statements-none)
    (production :case-statements (:case-label) case-statements-one)
    (production :case-statements (:case-label :case-statements-prefix (:case-statement abbrev)) case-statements-more)
    
    (production :case-statements-prefix () case-statements-prefix-none)
    (production :case-statements-prefix (:case-statements-prefix (:case-statement full)) case-statements-prefix-more)
    
    (production (:case-statement :omega_2) ((:substatement :omega_2)) case-statement-statement)
    (production (:case-statement :omega_2) (:case-label) case-statement-case-label)
    
    (production :case-label (case (:list-expression allow-in) \:) case-label-case)
    (production :case-label (default \:) case-label-default)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Do-While Statement")
    (production :do-statement (do (:substatement abbrev) while :paren-list-expression) do-statement-do-while)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "While Statement")
    (production (:while-statement :omega) (while :paren-list-expression (:substatement :omega)) while-statement-while)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "For Statements")
    (production (:for-statement :omega) (for \( :for-initialiser \; :optional-expression \; :optional-expression \)
                                             (:substatement :omega)) for-statement-c-style)
    (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:substatement :omega)) for-statement-in)
    
    (production :for-initialiser () for-initialiser-empty)
    (production :for-initialiser ((:list-expression no-in)) for-initialiser-expression)
    (production :for-initialiser (:attributes :variable-definition-kind (:variable-binding-list no-in)) for-initialiser-variable-definition)
    
    (production :for-in-binding (:postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:attributes :variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "With Statement")
    (production (:with-statement :omega) (with :paren-list-expression (:substatement :omega)) with-statement-with)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Continue and Break Statements")
    (rule :continue-statement ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((validate (v :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-continue d ""))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((validate (v :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-continue d (name :identifier))))))
    
    (rule :break-statement ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((validate (v :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-break d ""))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((validate (v :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-break d (name :identifier))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Return Statement")
    (rule :return-statement ((validate (-> (validation-env) void)) (eval (-> (dynamic-env) object)))
      (production :return-statement (return) return-statement-default
        ((validate v)
         (when (not (& can-return v))
           (throw syntax-error)))
        ((eval (e :unused)) (throw (new go-return undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((validate v)
         (when (not (& can-return v))
           (throw syntax-error))
         ((validate :list-expression) v))
        ((eval e) (throw (new go-return (read-reference ((eval :list-expression) e)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Throw Statement")
    (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Try Statement")
    (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses)
    (production :try-statement (try :block :finally-clause) try-statement-finally-clause)
    (production :try-statement (try :block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
    
    (production :catch-clauses (:catch-clause) catch-clauses-one)
    (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
    
    (production :catch-clause (catch \( :parameter \) :block) catch-clause-block)
    
    (production :finally-clause (finally :block) finally-clause-block)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 1 "Directives")
    (rule (:directive :omega_2) ((validate (-> (validation-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        ((validate v) ((validate :statement) v))
        ((eval e d) (return ((eval :statement) e d))))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:directive :omega_2) (:attribute :no-line-break :attributes (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:directive :omega_2) (:package-definition) directive-package-definition
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((validate (v :unused)) (todo))
          ((eval (e :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((validate (v :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    
    (production (:annotatable-directive :omega_2) (:export-definition (:semicolon :omega_2)) annotatable-directive-export-definition)
    (production (:annotatable-directive :omega_2) (:variable-definition (:semicolon :omega_2)) annotatable-directive-variable-definition)
    (production (:annotatable-directive :omega_2) ((:function-definition :omega_2)) annotatable-directive-function-definition)
    (production (:annotatable-directive :omega_2) ((:class-definition :omega_2)) annotatable-directive-class-definition)
    (production (:annotatable-directive :omega_2) (:namespace-definition (:semicolon :omega_2)) annotatable-directive-namespace-definition)
    (? js2
      (production (:annotatable-directive :omega_2) ((:interface-definition :omega_2)) annotatable-directive-interface-definition))
    (production (:annotatable-directive :omega_2) (:use-directive (:semicolon :omega_2)) annotatable-directive-use-directive)
    (production (:annotatable-directive :omega_2) (:import-directive (:semicolon :omega_2)) annotatable-directive-import-directive)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Attributes")
    (production :attributes () attributes-none)
    (production :attributes (:attribute :no-line-break :attributes) attributes-more)
    
    (production :attribute (:attribute-expression) attribute-attribute-expression)
    (production :attribute (abstract) attribute-abstract)
    (production :attribute (final) attribute-final)
    (production :attribute (private) attribute-private)
    (production :attribute (public) attribute-public)
    (production :attribute (static) attribute-static)
    (production :attribute (true) attribute-true)
    (production :attribute (false) attribute-false)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    
    (%heading 2 "Use Directive")
    (production :use-directive (use namespace :paren-list-expression :includes-excludes) use-directive-normal)
    
    (production :includes-excludes () includes-excludes-none)
    (production :includes-excludes (\, exclude \( :name-patterns \)) includes-excludes-exclude-list)
    (production :includes-excludes (\, include \( :name-patterns \)) includes-excludes-include-list)
    
    (production :name-patterns () name-patterns-empty)
    (production :name-patterns (:name-pattern-list) name-patterns-name-pattern-list)
    
    (production :name-pattern-list (:identifier) name-pattern-list-one)
    (production :name-pattern-list (:name-pattern-list \, :identifier) name-pattern-list-more)
    
    #|
    (production :name-patterns (:name-pattern) name-patterns-one)
    (production :name-patterns (:name-patterns \, :name-pattern) name-patterns-more)
    
    (production :name-pattern (:qualified-wildcard-pattern) name-pattern-qualified-wildcard-pattern)
    (production :name-pattern (:full-postfix-expression \. :qualified-wildcard-pattern) name-pattern-dot-qualified-wildcard-pattern)
    (production :name-pattern (:attribute-expression \. :qualified-wildcard-pattern) name-pattern-dot-qualified-wildcard-pattern2)
    
    (production :qualified-wildcard-pattern (:qualified-identifier) qualified-wildcard-pattern-qualified-identifier)
    (production :qualified-wildcard-pattern (:wildcard-pattern) qualified-wildcard-pattern-wildcard-pattern)
    (production :qualified-wildcard-pattern (:qualifier \:\: :wildcard-pattern) qualified-wildcard-pattern-qualifier)
    (production :qualified-wildcard-pattern (:paren-expression \:\: :wildcard-pattern) qualified-wildcard-pattern-expression-qualifier)
    
    (production :wildcard-pattern (*) wildcard-pattern-all)
    (production :wildcard-pattern ($regular-expression) wildcard-pattern-regular-expression)
    |#
    
    
    (%heading 2 "Import Directive")
    (production :import-directive (import :import-binding :includes-excludes) import-directive-import)
    (production :import-directive (import :import-binding \, namespace :paren-list-expression :includes-excludes)
                import-directive-import-namespaces)
    
    (production :import-binding (:import-source) import-binding-import-source)
    (production :import-binding (:identifier = :import-source) import-binding-named-import-source)
    
    (production :import-source ($string) import-source-string)
    (production :import-source (:package-name) import-source-package-name)
    
    
    (? js2
      (%heading 2 "Include Directive")
      (production :include-directive (include :no-line-break $string) include-directive-include))
    
    
    (%heading 2 "Pragma")
    (production :pragma (use :pragma-items) pragma-pragma-items)
    
    (production :pragma-items (:pragma-item) pragma-items-one)
    (production :pragma-items (:pragma-items \, :pragma-item) pragma-items-more)
    
    (production :pragma-item (:pragma-expr) pragma-item-pragma-expr)
    (production :pragma-item (:pragma-expr \?) pragma-item-optional-pragma-expr)
    
    (production :pragma-expr (:identifier) pragma-expr-identifier)
    (production :pragma-expr (:identifier :paren-list-expression) pragma-expr-identifier-and-arguments)
    
    
    (%heading 1 "Definitions")
    (%heading 2 "Export Definition")
    (production :export-definition (export :export-binding-list) export-definition-definition)
    
    (production :export-binding-list (:export-binding) export-binding-list-one)
    (production :export-binding-list (:export-binding-list \, :export-binding) export-binding-list-more)
    
    (production :export-binding (:function-name) export-binding-simple)
    (production :export-binding (:function-name = :function-name) export-binding-initialiser)
    
    
    (%heading 2 "Variable Definition")
    (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition)
    
    (production :variable-definition-kind (var) variable-definition-kind-var)
    (production :variable-definition-kind (const) variable-definition-kind-const)
    
    (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one)
    (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more)
    
    (production (:variable-binding :beta) ((:typed-identifier :beta)) variable-binding-simple)
    (production (:variable-binding :beta) ((:typed-identifier :beta) = (:variable-initialiser :beta)) variable-binding-initialised)
    
    (production (:variable-initialiser :beta) ((:assignment-expression :beta)) variable-initialiser-assignment-expression)
    (production (:variable-initialiser :beta) (:multiple-attributes) variable-initialiser-multiple-attributes)
    (production (:variable-initialiser :beta) (abstract) variable-initialiser-abstract)
    (production (:variable-initialiser :beta) (final) variable-initialiser-final)
    (production (:variable-initialiser :beta) (private) variable-initialiser-private)
    (production (:variable-initialiser :beta) (static) variable-initialiser-static)
    
    (production :multiple-attributes (:attribute :no-line-break :attribute) multiple-attributes-two)
    (production :multiple-attributes (:multiple-attributes :no-line-break :attribute) multiple-attributes-more)
    
    (production (:typed-identifier :beta) (:identifier) typed-identifier-identifier)
    (production (:typed-identifier :beta) (:identifier \: (:type-expression :beta)) typed-identifier-identifier-and-type)
    ;(production (:typed-identifier :beta) ((:type-expression :beta) :identifier) typed-identifier-type-and-identifier)
    
    
    (production :simple-variable-definition (var :untyped-variable-binding-list) simple-variable-definition-definition)
    
    (production :untyped-variable-binding-list (:untyped-variable-binding) untyped-variable-binding-list-one)
    (production :untyped-variable-binding-list (:untyped-variable-binding-list \, :untyped-variable-binding) untyped-variable-binding-list-more)
    
    (production :untyped-variable-binding (:identifier) untyped-variable-binding-simple)
    (production :untyped-variable-binding (:identifier = (:variable-initialiser allow-in)) untyped-variable-binding-initialised)
    
    
    (%heading 2 "Function Definition")
    (production (:function-definition :omega_2) (:function-declaration :block) function-definition-definition)
    (production (:function-definition :omega_2) (:function-declaration (:semicolon :omega_2)) function-definition-declaration)
    
    (production :function-declaration (function :function-name :function-signature) function-declaration-signature-and-body)
    
    (production :function-name (:identifier) function-name-function)
    (production :function-name (get :no-line-break :identifier) function-name-getter)
    (production :function-name (set :no-line-break :identifier) function-name-setter)
    (production :function-name ($string) function-name-string)
    
    (production :function-signature (:parameter-signature :result-signature) function-signature-parameter-and-result-signatures)
    
    (production :parameter-signature (\( :parameters \)) parameter-signature-parameters)
    
    (production :parameters () parameters-none)
    (production :parameters (:all-parameters) parameters-all-parameters)
    
    (production :all-parameters (:parameter) all-parameters-parameter)
    (production :all-parameters (:parameter \, :all-parameters) all-parameters-parameter-and-more)
    (production :all-parameters (:optional-parameters) all-parameters-optional-parameters)
    
    (production :optional-parameters (:optional-parameter) optional-parameters-optional-parameter)
    (production :optional-parameters (:optional-parameter \, :optional-parameters) optional-parameters-optional-parameter-and-more)
    (production :optional-parameters (:rest-and-named-parameters) optional-parameters-rest-and-named-parameters)
    
    (production :rest-and-named-parameters (:named-parameters) rest-and-named-parameters-named-parameters)
    (production :rest-and-named-parameters (:rest-parameter) rest-and-named-parameters-rest-parameter)
    (production :rest-and-named-parameters (:rest-parameter \, :named-parameters) rest-and-named-parameters-rest-and-named-parameters)
    (production :rest-and-named-parameters (:named-rest-parameter) rest-and-named-parameters-named-rest-parameter)
    
    (production :named-parameters (:named-parameter) named-parameters-named-parameter)
    (production :named-parameters (:named-parameter \, :named-parameters) named-parameters-named-parameter-and-more)
    
    (production :parameter ((:typed-identifier allow-in)) parameter-typed-identifier)
    (production :parameter (const (:typed-identifier allow-in)) parameter-const-typed-identifier)
    
    (production :optional-parameter (:parameter = (:assignment-expression allow-in)) optional-parameter-assignment-expression)
    
    (production :typed-initialiser ((:typed-identifier allow-in) = (:assignment-expression allow-in)) typed-initialiser-assignment-expression)
    
    (production :named-parameter (named :typed-initialiser) named-parameter-named-typed-initialiser)
    (production :named-parameter (const named :typed-initialiser) named-parameter-const-named-typed-initialiser)
    (production :named-parameter (named const :typed-initialiser) named-parameter-named-const-typed-initialiser)
    
    (production :rest-parameter (\.\.\.) rest-parameter-none)
    (production :rest-parameter (\.\.\. :parameter) rest-parameter-parameter)
    
    (production :named-rest-parameter (\.\.\. named :identifier) named-rest-parameter-named-identifier)
    (production :named-rest-parameter (\.\.\. const named :identifier) named-rest-parameter-const-named-identifier)
    (production :named-rest-parameter (\.\.\. named const :identifier) named-rest-parameter-named-const-identifier)
    
    (production :result-signature () result-signature-none)
    (production :result-signature (\: (:type-expression allow-in)) result-signature-colon-and-type-expression)
    ;(production :result-signature ((:- {) (:type-expression allow-in)) result-signature-type-expression)
    
    
    (%heading 2 "Class Definition")
    (production (:class-definition :omega_2) (class :identifier :inheritance :block) class-definition-definition)
    (production (:class-definition :omega_2) (class :identifier (:semicolon :omega_2)) class-definition-declaration)
    
    (production :inheritance () inheritance-none)
    (production :inheritance (extends (:type-expression allow-in)) inheritance-extends)
    (? js2
      (production :inheritance (implements :type-expression-list) inheritance-implements)
      (production :inheritance (extends (:type-expression allow-in) implements :type-expression-list) inheritance-extends-implements)
      
      (%heading 2 "Interface Definition")
      (production (:interface-definition :omega_2) (interface :identifier :extends-list :block) interface-definition-definition)
      (production (:interface-definition :omega_2) (interface :identifier (:semicolon :omega_2)) interface-definition-declaration)
      
      (production :extends-list () extends-list-none)
      (production :extends-list (extends :type-expression-list) extends-list-one)
      
      (production :type-expression-list ((:type-expression allow-in)) type-expression-list-one)
      (production :type-expression-list (:type-expression-list \, (:type-expression allow-in)) type-expression-list-more))
    
    
    (%heading 2 "Namespace Definition")
    (production :namespace-definition (namespace :identifier) namespace-definition-normal)
    
    
    (%heading 2 "Package Definition")
    (production :package-definition (package :block) package-definition-anonymous)
    (production :package-definition (package :package-name :block) package-definition-named)
    
    (production :package-name (:identifier) package-name-one)
    (production :package-name (:package-name \. :identifier) package-name-more)
    
    
    (%heading 1 "Programs")
    (rule :program ((eval-program object))
      (production :program (:directives) program-directives
        (eval-program
         (begin
          ((validate :directives) initial-validation-env)
          (return ((eval :directives) initial-dynamic-env undefined))))))
    
    
    
    (%heading (1 :semantics) "Predefined Identifiers")
    (%heading (1 :semantics) "Built-in Classes")
    (%heading (1 :semantics) "Built-in Functions")
    (%heading (1 :semantics) "Built-in Attributes")
    (%heading (1 :semantics) "Built-in Operators")
    (%heading (2 :semantics) "Unary Operators")
    
    (define (plus-object (this object :unused) (a object) (args argument-list :unused)) object
      (return (to-number a)))
    
    (define (minus-object (this object :unused) (a object) (args argument-list :unused)) object
      (return (float64-negate (to-number a))))
    
    (define (bitwise-not-object (this object :unused) (a object) (args argument-list :unused)) object
      (const i integer (to-int32 (to-number a)))
      (return (real-to-float64 (bitwise-xor i -1))))
    
    (define (increment-object (this object :unused) (a object) (args argument-list :unused)) object
      (const x object (unary-plus a))
      (return (binary-dispatch add-table null null x 1.0)))
    
    (define (decrement-object (this object :unused) (a object) (args argument-list :unused)) object
      (const x object (unary-plus a))
      (return (binary-dispatch subtract-table null null x 1.0)))
    
    (define (call-object (this object) (a object) (args argument-list)) object
      (case a
        (:select (union undefined null boolean float64 string namespace attribute prototype) (throw type-error))
        (:narrow (union class instance) (return ((& call a) this args)))
        (:narrow method-closure (return (call-object (& this a) (& f (& method a)) args)))))
    
    (define (construct-object (this object) (a object) (args argument-list)) object
      (case a
        (:select (union undefined null boolean float64 string namespace attribute method-closure prototype) (throw type-error))
        (:narrow (union class instance) (return ((& construct a) this args)))))
    
    (define (bracket-read-object (this object :unused) (a object) (args argument-list)) object
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0)))
      (return (read-qualified-property a name public-namespace true)))
    
    (define (bracket-write-object (this object :unused) (a object) (args argument-list)) object
      (rwhen (or (/= (length (& positional args)) 2) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const new-value object (nth (& positional args) 0))
      (const name string (to-string (nth (& positional args) 1)))
      (write-qualified-property a name public-namespace true new-value)
      (return undefined))
    
    (define (bracket-delete-object (this object :unused) (a object) (args argument-list)) object
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0)))
      (return (delete-qualified-property a name public-namespace true)))
    
    
    (defvar plus-table (list-set unary-method) (list-set (new unary-method object-class plus-object)))
    (defvar minus-table (list-set unary-method) (list-set (new unary-method object-class minus-object)))
    (defvar bitwise-not-table (list-set unary-method) (list-set (new unary-method object-class bitwise-not-object)))
    (defvar increment-table (list-set unary-method) (list-set (new unary-method object-class increment-object)))
    (defvar decrement-table (list-set unary-method) (list-set (new unary-method object-class decrement-object)))
    (defvar call-table (list-set unary-method) (list-set (new unary-method object-class call-object)))
    (defvar construct-table (list-set unary-method) (list-set (new unary-method object-class construct-object)))
    (defvar bracket-read-table (list-set unary-method) (list-set (new unary-method object-class bracket-read-object)))
    (defvar bracket-write-table (list-set unary-method) (list-set (new unary-method object-class bracket-write-object)))
    (defvar bracket-delete-table (list-set unary-method) (list-set (new unary-method object-class bracket-delete-object)))
    
    
    (%heading (2 :semantics) "Binary Operators")
    
    (define (add-objects (a object) (b object)) object
      (const ap object (to-primitive a null))
      (const bp object (to-primitive b null))
      (if (or (in ap string) (in bp string))
        (return (append (to-string ap) (to-string bp)))
        (return (float64-add (to-number ap) (to-number bp)))))
    
    (define (subtract-objects (a object) (b object)) object
      (return (float64-subtract (to-number a) (to-number b))))
    
    (define (multiply-objects (a object) (b object)) object
      (return (float64-multiply (to-number a) (to-number b))))
    
    (define (divide-objects (a object) (b object)) object
      (return (float64-divide (to-number a) (to-number b))))
    
    (define (remainder-objects (a object) (b object)) object
      (return (float64-remainder (to-number a) (to-number b))))
    
    
    (define (less-objects (a object) (b object)) object
      (const ap object (to-primitive a null))
      (const bp object (to-primitive b null))
      (if (and (in ap string :narrow-true) (in bp string :narrow-true))
        (return (< ap bp string))
        (return (= (float64-compare (to-number ap) (to-number bp)) less order))))
    
    (define (less-or-equal-objects (a object) (b object)) object
      (const ap object (to-primitive a null))
      (const bp object (to-primitive b null))
      (if (and (in ap string :narrow-true) (in bp string :narrow-true))
        (return (<= ap bp string))
        (return (in (float64-compare (to-number ap) (to-number bp)) (tag less equal)))))
    
    (define (equal-objects (a object) (b object)) object
      (case a
        (:select (union undefined null)
          (return (in b (union undefined null))))
        (:narrow boolean
          (if (in b boolean :narrow-true)
            (return (= a b boolean))
            (return (equal-objects (to-number a) b))))
        (:narrow float64
          (const bp object (to-primitive b null))
          (case bp
            (:select (union undefined null namespace attribute class method-closure prototype instance) (return false))
            (:select (union boolean string float64) (return (= (float64-compare a (to-number bp)) equal order)))))
        (:narrow string
          (const bp object (to-primitive b null))
          (case bp
            (:select (union undefined null namespace attribute class method-closure prototype instance) (return false))
            (:select (union boolean float64) (return (= (float64-compare (to-number a) (to-number bp)) equal order)))
            (:narrow string (return (= a bp string)))))
        (:select (union namespace attribute class method-closure prototype instance)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace attribute class method-closure prototype instance) (return (strict-equal-objects a b)))
            (:select (union boolean float64 string)
              (const ap object (to-primitive a null))
              (case ap
                (:select (union undefined null namespace attribute class method-closure prototype instance) (return false))
                (:select (union boolean float64 string) (return (equal-objects ap b)))))))))
    
    (define (strict-equal-objects (a object) (b object)) object
      (if (and (in a float64 :narrow-true) (in b float64 :narrow-true))
        (return (= (float64-compare a b) equal order))
        (return (= a b object))))
    
    
    (define (shift-left-objects (a object) (b object)) object
      (const i integer (to-u-int32 (to-number a)))
      (const count integer (bitwise-and (to-u-int32 (to-number b)) (hex #x1F)))
      (return (real-to-float64 (u-int32-to-int32 (bitwise-and (bitwise-shift i count) (hex #xFFFFFFFF))))))
    
    (define (shift-right-objects (a object) (b object)) object
      (const i integer (to-int32 (to-number a)))
      (const count integer (bitwise-and (to-u-int32 (to-number b)) (hex #x1F)))
      (return (real-to-float64 (bitwise-shift i (neg count)))))
    
    (define (shift-right-unsigned-objects (a object) (b object)) object
      (const i integer (to-u-int32 (to-number a)))
      (const count integer (bitwise-and (to-u-int32 (to-number b)) (hex #x1F)))
      (return (real-to-float64 (bitwise-shift i (neg count)))))
    
    (define (bitwise-and-objects (a object) (b object)) object
      (const i integer (to-int32 (to-number a)))
      (const j integer (to-int32 (to-number b)))
      (return (real-to-float64 (bitwise-and i j))))
    
    (define (bitwise-xor-objects (a object) (b object)) object
      (const i integer (to-int32 (to-number a)))
      (const j integer (to-int32 (to-number b)))
      (return (real-to-float64 (bitwise-xor i j))))
    
    (define (bitwise-or-objects (a object) (b object)) object
      (const i integer (to-int32 (to-number a)))
      (const j integer (to-int32 (to-number b)))
      (return (real-to-float64 (bitwise-or i j))))
    
    
    (defvar add-table (list-set binary-method) (list-set (new binary-method object-class object-class add-objects)))
    (defvar subtract-table (list-set binary-method) (list-set (new binary-method object-class object-class subtract-objects)))
    (defvar multiply-table (list-set binary-method) (list-set (new binary-method object-class object-class multiply-objects)))
    (defvar divide-table (list-set binary-method) (list-set (new binary-method object-class object-class divide-objects)))
    (defvar remainder-table (list-set binary-method) (list-set (new binary-method object-class object-class remainder-objects)))
    (defvar less-table (list-set binary-method) (list-set (new binary-method object-class object-class less-objects)))
    (defvar less-or-equal-table (list-set binary-method) (list-set (new binary-method object-class object-class less-or-equal-objects)))
    (defvar equal-table (list-set binary-method) (list-set (new binary-method object-class object-class equal-objects)))
    (defvar strict-equal-table (list-set binary-method) (list-set (new binary-method object-class object-class strict-equal-objects)))
    (defvar shift-left-table (list-set binary-method) (list-set (new binary-method object-class object-class shift-left-objects)))
    (defvar shift-right-table (list-set binary-method) (list-set (new binary-method object-class object-class shift-right-objects)))
    (defvar shift-right-unsigned-table (list-set binary-method) (list-set (new binary-method object-class object-class shift-right-unsigned-objects)))
    (defvar bitwise-and-table (list-set binary-method) (list-set (new binary-method object-class object-class bitwise-and-objects)))
    (defvar bitwise-xor-table (list-set binary-method) (list-set (new binary-method object-class object-class bitwise-xor-objects)))
    (defvar bitwise-or-table (list-set binary-method) (list-set (new binary-method object-class object-class bitwise-or-objects)))
    
    
    (%heading (1 :semantics) "Built-in Namespaces")
    (%heading (1 :semantics) "Built-in Units")
    ))


(defparameter *jw* (generate-world "J" *jw-source* '((js2 . :js2) (es4 . :es4))))
(defparameter *jg* (world-grammar *jw* 'code-grammar))
(ensure-lf-subset *jg*)
(forward-parser-states *jg*)
#+allegro (clean-grammar *jg*) ;Remove this line if you wish to print the grammar's state tables.

(defparameter *ew* nil)
(defparameter *eg* nil)

(defun compute-ecma-subset ()
  (unless *ew*
    (setq *ew* (generate-world "E" *jw-source* '((js2 . delete) (es4 . nil))))
    (setq *eg* (world-grammar *ew* 'code-grammar))
    (ensure-lf-subset *eg*)
    (forward-parser-states *eg*))
  (length (grammar-states *eg*)))


; Print a list of states that have both $REGULAR-EXPRESSION and either / or /= as valid lookaheads.
(defun show-regexp-and-division-states (grammar)
  (all-state-transitions
   #'(lambda (state transitions-hash)
       (when (and (gethash '$regular-expression transitions-hash)
                  (or (gethash '/ transitions-hash) (gethash '/= transitions-hash)))
         (format *error-output* "State ~S~%" state)))
   grammar))


; Return five values:
;   A list of terminals that may precede a $regular-expression terminal;
;   A list of terminals that may precede a $virtual-semicolon but not / or /= terminal;
;   A list of terminals that may precede a / or /= terminal;
;   The intersection of the $regular-expression and /|/= lists.
;   The intersection of the $regular-expression|$virtual-semicolon and /|/= lists.
;
; USE ONLY ON canonical-lr-1 grammars.
; DON'T RUN THIS AFTER CALLING forward-parser-states.
(defun show-regexp-and-division-predecessors (grammar)
  (let* ((nstates (length (grammar-states grammar)))
         (state-predecessors (make-array nstates :element-type 'terminalset :initial-element *empty-terminalset*)))
    (dolist (state (grammar-states grammar))
      (dolist (transition-pair (state-transitions state))
        (let ((transition (cdr transition-pair)))
          (when (eq (transition-kind transition) :shift)
            (terminalset-union-f (svref state-predecessors (state-number (transition-state transition)))
                                 (make-terminalset grammar (car transition-pair)))))))
    (let ((regexp-predecessors *empty-terminalset*)
          (virtual-predecessors *empty-terminalset*)
          (div-predecessors *empty-terminalset*))
      (all-state-transitions
       #'(lambda (state transitions-hash)
           (let ((predecessors (svref state-predecessors (state-number state))))
             (when (gethash '$regular-expression transitions-hash)
               (terminalset-union-f regexp-predecessors predecessors))
             (if (or (gethash '/ transitions-hash) (gethash '/= transitions-hash))
               (terminalset-union-f div-predecessors predecessors)
               (when (gethash '$virtual-semicolon transitions-hash)
                 (terminalset-union-f virtual-predecessors predecessors)))))
       grammar)
      (values
       (terminalset-list grammar regexp-predecessors)
       (terminalset-list grammar virtual-predecessors)
       (terminalset-list grammar div-predecessors)
       (terminalset-list grammar (terminalset-intersection regexp-predecessors div-predecessors))
       (terminalset-list grammar (terminalset-intersection (terminalset-union regexp-predecessors virtual-predecessors) div-predecessors))))))


(defun depict-js-terminals (markup-stream grammar heading)
  (labels
    ((production-first-terminal (production)
       (first (production-rhs production)))
     
     (terminal-bin (terminal)
       (if (and terminal (symbolp terminal))
         (let ((name (symbol-name terminal)))
           (if (> (length name) 0)
             (let ((first-char (char name 0)))
               (cond
                ((char= first-char #\$) 0)
                ((not (or (char= first-char #\_) (alphanumericp first-char))) 1)
                ((member terminal (rule-productions (grammar-rule grammar :identifier)) :key #'production-first-terminal) 5)
                (t 3)))
             1))
         1))
     
     (depict-terminal-bin (bin-name bin-terminals)
       (when bin-terminals
         (depict-paragraph (markup-stream :body-text)
           (depict markup-stream bin-name)
           (depict-list markup-stream #'depict-terminal bin-terminals :separator " ")))))
    
    (let* ((bins (make-array 6 :initial-element nil))
           (all-terminals (grammar-terminals grammar))
           (terminals (remove-if #'lf-terminal? all-terminals)))
      (assert-true (= (length all-terminals) (1- (* 2 (length terminals)))))
      (setf (svref bins 2) (list '\# '&&= '-> '@ '^^ '^^= '\|\|=))
      (setf (svref bins 4) (list 'abstract 'class 'const 'debugger 'enum 'export 'extends 'final 'goto 'implements 'import
                                 'instanceof 'interface 'native 'package 'private 'protected 'public 'static 'super 'synchronized
                                 'throws 'transient 'volatile))
      ; Used to be reserved in JavaScript 1.5: 'boolean 'byte 'char 'double 'float 'int 'long 'short
      (do ((i (length terminals)))
          ((zerop i))
        (let ((terminal (aref terminals (decf i))))
          (unless (eq terminal *end-marker*)
            (setf (svref bins 2) (delete terminal (svref bins 2)))
            (setf (svref bins 4) (delete terminal (svref bins 4)))
            (push terminal (svref bins (terminal-bin terminal))))))
      (depict-paragraph (markup-stream heading)
        (depict-link (markup-stream :definition "terminals" "" nil)
          (depict markup-stream "Terminals")))
      (mapc #'depict-terminal-bin '("General tokens: " "Punctuation tokens: " "Future punctuation tokens: "
                                    "Reserved words: " "Future reserved words: " "Non-reserved words: ")
            (coerce bins 'list)))))


(defun dump-parser ()
  (values
   (length (grammar-states *jg*))
   (depict-rtf-to-local-file
    "JS20/ParserGrammarJS2.rtf"
    "JavaScript 2.0 Syntactic Grammar"
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *jg* :heading1)
        (depict-world-commands markup-stream *jw* :visible-semantics nil)))
   (depict-rtf-to-local-file
    "JS20/ParserSemanticsJS2.rtf"
    "JavaScript 2.0 Syntactic Semantics"
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *jg* :heading1)
        (depict-world-commands markup-stream *jw*)))
   (compute-ecma-subset)
   (depict-rtf-to-local-file
    "JS20/ParserGrammarES4.rtf"
    "ECMAScript Edition 4 Syntactic Grammar"
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *eg* :heading1)
        (depict-world-commands markup-stream *ew* :visible-semantics nil)))
   (depict-rtf-to-local-file
    "JS20/ParserSemanticsES4.rtf"
    "ECMAScript Edition 4 Syntactic Semantics"
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *eg* :heading1)
        (depict-world-commands markup-stream *ew*)))
   
   (length (grammar-states *jg*))
   (depict-html-to-local-file
    "JS20/ParserGrammarJS2.html"
    "JavaScript 2.0 Syntactic Grammar"
    t
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *jg* :heading2)
        (depict-world-commands markup-stream *jw* :heading-offset 1 :visible-semantics nil))
    :external-link-base "notation.html")
   (depict-html-to-local-file
    "JS20/ParserSemanticsJS2.html"
    "JavaScript 2.0 Syntactic Semantics"
    t
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *jg* :heading1)
        (depict-world-commands markup-stream *jw*))
    :external-link-base "notation.html")
   (compute-ecma-subset)
   (depict-html-to-local-file
    "JS20/ParserGrammarES4.html"
    "ECMAScript Edition 4 Syntactic Grammar"
    t
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *eg* :heading2)
        (depict-world-commands markup-stream *ew* :heading-offset 1 :visible-semantics nil))
    :external-link-base "notation.html")
   (depict-html-to-local-file
    "JS20/ParserSemanticsES4.html"
    "ECMAScript Edition 4 Syntactic Semantics"
    t
    #'(lambda (markup-stream)
        (depict-js-terminals markup-stream *eg* :heading1)
        (depict-world-commands markup-stream *ew*))
    :external-link-base "notation.html")))


#|
(dump-parser)

(depict-rtf-to-local-file
 "JS20/ParserSemanticsJS2.rtf"
 "JavaScript 2.0 Syntactic Semantics"
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg* :heading1)
     (depict-world-commands markup-stream *jw*)))

(depict-html-to-local-file
 "JS20/ParserSemanticsJS2.html"
 "JavaScript 2.0 Syntactic Semantics"
 t
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg* :heading1)
     (depict-world-commands markup-stream *jw*))
 :external-link-base "notation.html")


(with-local-output (s "JS20/ParserGrammarJS2 states") (print-grammar *jg* s))
(compute-ecma-subset)
(with-local-output (s "JS20/ParserGrammarES4 states") (print-grammar *eg* s))
|#

(length (grammar-states *jg*))
