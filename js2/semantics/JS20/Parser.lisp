;;;
;;; JavaScript 2.0 parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(defparameter *jw-source* 
  '((line-grammar code-grammar :lr-1 :program)
    
    (%subsection :semantics "Errors")
    
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
    
    
    (%subsection :semantics "Undefined")
    (deftag undefined)
    (deftype undefined (tag undefined))
    
    
    (%subsection :semantics "Null")
    (deftag null)
    (deftype null (tag null))
    
    
    (%subsection :semantics "Namespaces")
    (defrecord namespace (name string))
    (deftype namespace-opt (union null namespace))
    
    (define public-namespace namespace (new namespace "public"))
    
    
    (%subsection :semantics "Attributes")
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
    
    
    (%subsection :semantics "Classes")
    (%text :comment "The first " (:type object) " is the this value, the " (:type (vector object)) " are the positional arguments, and the "
           (:type (list-set named-argument)) " are the named arguments.")
    (deftype invoker (-> (object (vector object) (list-set named-argument)) object))
    
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
      (function (call (this object :unused) (positional-args (vector object) :unused) (named-args (list-set named-argument) :unused)) object
        (todo))
      (function (construct (this object :unused) (positional-args (vector object) :unused) (named-args (list-set named-argument) :unused)) object
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
    
    
    (%subsection :semantics "Method Closures")
    (deftuple method-closure
      (this object)
      (method method))
    
    
    (%subsection :semantics "General Instances")
    (defrecord instance
      (type class)
      (model instance-opt)
      (call invoker)
      (construct invoker)
      (typeof-string string)
      (slots (list-set slot) :var)
      (dynamic-properties (list-set dynamic-property) :var))
    (deftype instance-opt (union null instance))
    
    (defrecord dynamic-property 
      (name string)
      (value object))
    
    
    (%subsection :semantics "Objects")
    
    (deftype object (union undefined null boolean float64 string namespace attribute class method-closure instance))
    
    
    (%text :comment "Return " (:local o) :apostrophe "s most specific type.")
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
        (:narrow instance (return (& type o)))))
    
    (%text :comment "Return " (:tag true) " if " (:local o) " is an instance of class " (:local c) ". Consider "
           (:tag null) " to be an instance of the classes " (:character-literal "Null") " and "
           (:character-literal "Object") " only.")
    (define (instance-of (o object) (c class)) boolean
      (return (is-ancestor c (object-type o))))
    
    (%text :comment "Return " (:tag true) " if " (:local o) " is an instance of class " (:local c) ". Consider "
           (:tag null) " to be an instance of the classes " (:character-literal "Null") ", "
           (:character-literal "Object") ", and all other non-primitive classes.")
    (define (relaxed-instance-of (o object) (c class)) boolean
      (const t class (object-type o))
      (return (or (is-ancestor c t)
                  (and (= o null object) (not (& primitive c))))))
    
    (define (to-boolean (o object)) boolean
      (case o
        (:select (union undefined null) (return false))
        (:narrow boolean (return o))
        (:narrow float64 (return (not-in o (tag +zero -zero nan))))
        (:narrow string (return (/= o "" string)))
        (:select (union namespace attribute class method-closure) (return true))
        (:select instance (todo))))
    
    (define (to-number (o object)) float64
      (case o
        (:select undefined (return nan))
        (:select (union null (tag false)) (return +zero))
        (:select (tag true) (return 1.0))
        (:narrow float64 (return o))
        (:select string (todo))
        (:select (union namespace attribute class method-closure) (throw type-error))
        (:select instance (todo))))
    
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
        (:select instance (todo))))
    
    (define (to-primitive (o object) (hint object :unused)) object
      (case o
        (:select (union undefined null boolean float64 string) (return o))
        (:select (union namespace attribute class method-closure instance) (return (to-string o)))))
    
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
    
    
    (%subsection :semantics "References")
    (deftag ref)
    (deftype ref (tag ref))
    (deftype reference (union object ref))
    
    (%text :comment "Read the " (:type reference) ".")
    (define (read-reference (r reference)) object
      (case r
        (:narrow object (return r))
        (:select ref (todo))))
    
    (%text :comment "Write " (:local v) " into the " (:type reference) " " (:local r) ".")
    (define (write-reference (r reference) (v object :unused)) void
      (case r
        (:select object (throw reference-error))
        (:select ref (todo))))
    
    
    (%subsection :semantics "Slots")
    (defrecord slot-id (type class))
    
    (defrecord slot
      (id slot-id)
      (value object :var))
    
    (define (find-slot (o object) (id slot-id)) slot
      (rwhen (not-in o instance :narrow-false)
        (bottom))
      (const matching-slots (list-set slot)
        (map (& slots o) s s (= (& id s) id slot-id)))
      (assert (= (length matching-slots) 1))
      (return (elt-of matching-slots)))
    
    (defrecord global-slot
      (type class)
      (value object :var))
    
    
    (%subsection :semantics "Signatures")
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
    
    
    (%subsection :semantics "Members")
    (defrecord method
      (type signature)
      (f instance-opt)) ;Method code (may be undefined)
    
    (defrecord accessor
      (type class)
      (f instance)) ;Getter or setter function code
    
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
    
    (deftuple qualified-name (namespace namespace) (name string))
    
    
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
    
    (define (read-qualified-property (o object) (name string) (ns namespace) (indexable-only boolean)) object
      (when (in o instance :narrow-true)
        (reserve p)
        (rwhen (and (= ns public-namespace namespace)
                    (some (& dynamic-properties o) p (= name (& name p) string) :define-true))
          (return (& value p)))
        (rwhen (not-in (& model o) (tag null))
          (return (read-qualified-property (& model o) name ns indexable-only))))
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
          (return ((& call (& f d)) o (vector-of object) (list-set-of named-argument))))))
    
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
      (when (in o instance :narrow-true)
        (rwhen (not-in (& model o) (tag null))
          (return (resolve-object-namespace (& model o) name uses))))
      (const ns namespace-opt (if (in o class :narrow-true)
                                (resolve-member-namespace o true name uses)
                                (resolve-member-namespace (object-type o) false name uses)))
      (rwhen (not-in ns (tag null) :narrow-true)
        (return ns))
      (return public-namespace))
    
    (define (read-unqualified-property (o object) (name string) (uses (list-set namespace))) object
      (const ns namespace (resolve-object-namespace o name uses))
      (return (read-qualified-property o name ns false)))
    
    (define (write-qualified-property (o object :unused) (name string :unused) (ns namespace :unused) (indexable-only boolean :unused) (new-value object :unused)) void
      (todo))
    
    (define (delete-qualified-property (o object :unused) (name string :unused) (ns namespace :unused) (indexable-only boolean :unused)) boolean
      (todo))
    
    
    (%subsection :semantics "Verification Environments")
    (deftuple verify-env
      (enclosing-class class-opt)
      (labels (vector string))
      (can-return boolean)
      (constants (vector definition)))
    
    (define initial-verify-env verify-env (new verify-env null (vector-of string) false (vector-of definition)))
    
    (%text :comment "Return a " (:type verify-env) " with label " (:local label) " prepended to " (:local s) ".")
    (define (add-label (t verify-env) (label string)) verify-env
      (return (new verify-env (& enclosing-class t) (append (vector label) (& labels t)) (& can-return t) (& constants t))))
    
    (%text :comment "Return " (:tag true) " if this code is inside a class body.")
    (define (inside-class (s verify-env)) boolean
      (return (/= (& enclosing-class s) null class-opt)))
    
    
    (%subsection :semantics "Dynamic Environments")
    (defrecord dynamic-env
      (parent dynamic-env-opt)
      (enclosing-class class-opt)
      (reader-definitions (vector definition) :var)
      (reader-passthroughs (vector qualified-name) :var)
      (writer-definitions (vector definition) :var)
      (writer-passthroughs (vector qualified-name) :var))
    (deftype dynamic-env-opt (union null dynamic-env))
    
    (%text :comment "If the " (:type dynamic-env) " is from within a class" :apostrophe "s body, return that class; otherwise, return " (:tag null) ".")
    (define (lexical-class (e dynamic-env :unused)) class-opt
      (todo))
    
    (define initial-dynamic-env dynamic-env (new dynamic-env null null
                                                 (vector-of definition) (vector-of qualified-name)
                                                 (vector-of definition) (vector-of qualified-name)))
    
    
    (deftuple definition 
      (name qualified-name)
      (type class)
      (data (union slot object accessor)))
    
    
    (define (lookup-variable (e dynamic-env :unused) (name string :unused) (internal-is-namespace boolean :unused)) reference
      (todo))

    (define (lookup-qualified-variable (e dynamic-env :unused) (namespace namespace :unused) (name string :unused)) reference
      (todo))
    
    
    (%subsection :semantics "Unary Operators")
    (deftuple named-argument (name string) (value object))
    
    (deftuple unary-method 
      (operand-type class)
      (op (-> (object object (vector object) (list-set named-argument)) object)))
    
    (defrecord unary-table
      (methods (list-set unary-method) :var))
    
    (%text :comment "Return " (:tag true) " if " (:local v) " is a member of class " (:local c) " and, if "
           (:local limit) " is non-" (:tag null) ", " (:local c) " is a proper ancestor of " (:local limit) ".")
    (define (limited-instance-of (v object) (c class) (limit class-opt)) boolean
      (if (instance-of v c)
        (if (in limit (tag null) :narrow-false)
          (return true)
          (return (is-proper-ancestor c limit)))
        (return false)))
    
    (%text :comment "Dispatch the unary operator described by " (:local table) " applied to the " (:character-literal "this")
           " value " (:local this) ", the first argument " (:local op)
           ", a vector of zero or more additional positional arguments " (:local positional-args)
           ", and a set of zero or more named arguments " (:local named-args)
           ". If " (:local limit) " is non-" (:tag null)
           ", restrict the lookup to operators defined on the proper ancestors of " (:local limit) ".")
    (define (unary-dispatch (table unary-table) (limit class-opt) (this object) (op object) (positional-args (vector object))
                            (named-args (list-set named-argument))) object
      (const applicable-ops (list-set unary-method)
        (map (& methods table) m m (limited-instance-of op (& operand-type m) limit)))
      (reserve best)
      (if (some applicable-ops best
                (every applicable-ops m2 (is-ancestor (& operand-type m2) (& operand-type best))) :define-true)
        (return ((& op best) this op positional-args named-args))
        (throw property-not-found-error)))
    
    
    (%subsection :semantics "Unary Operator Tables")
    
    (define (plus-object (this object :unused) (a object) (positional-args (vector object) :unused) (named-args (list-set named-argument) :unused)) object
      (return (to-number a)))
    
    (define (minus-object (this object :unused) (a object) (positional-args (vector object) :unused) (named-args (list-set named-argument) :unused)) object
      (return (float64-negate (to-number a))))
    
    (define (bitwise-not-object (this object :unused) (a object) (positional-args (vector object) :unused) (named-args (list-set named-argument) :unused)) object
      (const i integer (to-int32 (to-number a)))
      (return (real-to-float64 (bitwise-xor i -1))))
    
    (define (increment-object (this object :unused) (a object) (positional-args (vector object) :unused) (named-args (list-set named-argument) :unused)) object
      (const x object (unary-plus a))
      (return (binary-dispatch add-table null null x 1.0)))
    
    (define (decrement-object (this object :unused) (a object) (positional-args (vector object) :unused) (named-args (list-set named-argument) :unused)) object
      (const x object (unary-plus a))
      (return (binary-dispatch subtract-table null null x 1.0)))
    
    (define (call-object (this object) (a object) (positional-args (vector object)) (named-args (list-set named-argument))) object
      (case a
        (:select (union undefined null boolean float64 string namespace attribute) (throw type-error))
        (:narrow (union class instance) (return ((& call a) this positional-args named-args)))
        (:narrow method-closure (return (call-object (& this a) (& f (& method a)) positional-args named-args)))))
    
    (define (construct-object (this object) (a object) (positional-args (vector object)) (named-args (list-set named-argument))) object
      (case a
        (:select (union undefined null boolean float64 string namespace attribute method-closure) (throw type-error))
        (:narrow (union class instance) (return ((& construct a) this positional-args named-args)))))
    
    (define (bracket-read-object (this object :unused) (a object) (positional-args (vector object)) (named-args (list-set named-argument))) object
      (rwhen (or (/= (length positional-args) 1) (nonempty named-args))
        (throw argument-mismatch-error))
      (const name string (to-string (nth positional-args 0)))
      (return (read-qualified-property a name public-namespace true)))
    
    (define (bracket-write-object (this object :unused) (a object) (positional-args (vector object)) (named-args (list-set named-argument))) object
      (rwhen (or (/= (length positional-args) 2) (nonempty named-args))
        (throw argument-mismatch-error))
      (const new-value object (nth positional-args 0))
      (const name string (to-string (nth positional-args 1)))
      (write-qualified-property a name public-namespace true new-value)
      (return new-value))
    
    (define (bracket-delete-object (this object :unused) (a object) (positional-args (vector object)) (named-args (list-set named-argument))) object
      (rwhen (or (/= (length positional-args) 1) (nonempty named-args))
        (throw argument-mismatch-error))
      (const name string (to-string (nth positional-args 0)))
      (return (delete-qualified-property a name public-namespace true)))
    
    
    (define plus-table unary-table (new unary-table (list-set (new unary-method object-class plus-object))))
    (define minus-table unary-table (new unary-table (list-set (new unary-method object-class minus-object))))
    (define bitwise-not-table unary-table (new unary-table (list-set (new unary-method object-class bitwise-not-object))))
    (define increment-table unary-table (new unary-table (list-set (new unary-method object-class increment-object))))
    (define decrement-table unary-table (new unary-table (list-set (new unary-method object-class decrement-object))))
    (define call-table unary-table (new unary-table (list-set (new unary-method object-class call-object))))
    (define construct-table unary-table (new unary-table (list-set (new unary-method object-class construct-object))))
    (define bracket-read-table unary-table (new unary-table (list-set (new unary-method object-class bracket-read-object))))
    (define bracket-write-table unary-table (new unary-table (list-set (new unary-method object-class bracket-write-object))))
    (define bracket-delete-table unary-table (new unary-table (list-set (new unary-method object-class bracket-delete-object))))
    
    
    (define (unary-plus (a object)) object
      (return (unary-dispatch plus-table null null a (vector-of object) (list-set-of named-argument))))
    
    (define (unary-not (a object)) object
      (return (not (to-boolean a))))
    
    
    (%subsection :semantics "Binary Operators")
    (deftuple binary-method
      (left-type class)
      (right-type class)
      (op (-> (object object) object)))
    
    (defrecord binary-table
      (methods (list-set binary-method) :var))
    
    
    (%text :comment "Return " (:tag true) " if " (:local m1) " is at least as specific as " (:local m2) ".")
    (define (is-binary-descendant (m1 binary-method) (m2 binary-method)) boolean
      (return (and (is-ancestor (& left-type m2) (& left-type m1))
                   (is-ancestor (& right-type m2) (& right-type m1)))))
    
    (%text :comment "Dispatch the binary operator described by " (:local table) " applied to " (:local left) " and " (:local right)
           ". If " (:local left-limit) " is non-" (:tag null)
           ", restrict the lookup to operator definitions with an ancestor of " (:local left-limit)
           " for the left operand. Similarly, if " (:local right-limit) " is non-" (:tag null)
           ", restrict the lookup to operator definitions with an ancestor of " (:local right-limit) " for the right operand.")
    (define (binary-dispatch (table binary-table) (left-limit class-opt) (right-limit class-opt) (left object) (right object)) object
      (const applicable-ops (list-set binary-method)
        (map (& methods table) m m (and (limited-instance-of left (& left-type m) left-limit)
                                        (limited-instance-of right (& right-type m) right-limit))))
      (reserve best)
      (if (some applicable-ops best
                (every applicable-ops m2 (is-binary-descendant best m2)) :define-true)
        (return ((& op best) left right))
        (throw property-not-found-error)))
    
    
    (%subsection :semantics "Binary Operator Tables")
    
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
            (:select (union undefined null namespace attribute class method-closure instance) (return false))
            (:select (union boolean string float64) (return (= (float64-compare a (to-number bp)) equal order)))))
        (:narrow string
          (const bp object (to-primitive b null))
          (case bp
            (:select (union undefined null namespace attribute class method-closure instance) (return false))
            (:select (union boolean float64) (return (= (float64-compare (to-number a) (to-number bp)) equal order)))
            (:narrow string (return (= a bp string)))))
        (:select (union namespace attribute class method-closure instance)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace attribute class method-closure instance) (return (strict-equal-objects a b)))
            (:select (union boolean float64 string)
              (const ap object (to-primitive a null))
              (case ap
                (:select (union undefined null namespace attribute class method-closure instance) (return false))
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
    
    
    (define add-table binary-table (new binary-table (list-set (new binary-method object-class object-class add-objects))))
    (define subtract-table binary-table (new binary-table (list-set (new binary-method object-class object-class subtract-objects))))
    (define multiply-table binary-table (new binary-table (list-set (new binary-method object-class object-class multiply-objects))))
    (define divide-table binary-table (new binary-table (list-set (new binary-method object-class object-class divide-objects))))
    (define remainder-table binary-table (new binary-table (list-set (new binary-method object-class object-class remainder-objects))))
    (define less-table binary-table (new binary-table (list-set (new binary-method object-class object-class less-objects))))
    (define less-or-equal-table binary-table (new binary-table (list-set (new binary-method object-class object-class less-or-equal-objects))))
    (define equal-table binary-table (new binary-table (list-set (new binary-method object-class object-class equal-objects))))
    (define strict-equal-table binary-table (new binary-table (list-set (new binary-method object-class object-class strict-equal-objects))))
    (define shift-left-table binary-table (new binary-table (list-set (new binary-method object-class object-class shift-left-objects))))
    (define shift-right-table binary-table (new binary-table (list-set (new binary-method object-class object-class shift-right-objects))))
    (define shift-right-unsigned-table binary-table (new binary-table (list-set (new binary-method object-class object-class shift-right-unsigned-objects))))
    (define bitwise-and-table binary-table (new binary-table (list-set (new binary-method object-class object-class bitwise-and-objects))))
    (define bitwise-xor-table binary-table (new binary-table (list-set (new binary-method object-class object-class bitwise-xor-objects))))
    (define bitwise-or-table binary-table (new binary-table (list-set (new binary-method object-class object-class bitwise-or-objects))))
    
    
    (%section "Terminal Actions")
    
    (declare-action name $identifier string 1)
    (declare-action eval $number float64 1)
    (declare-action eval $string string 1)
    
    (terminal-action name $identifier identity)
    (terminal-action eval $number identity)
    (terminal-action eval $string identity)
    (%print-actions)
    
    
    (%section "Expressions")
    (grammar-argument :beta allow-in no-in)
    
    (%subsection "Identifiers")
    (rule :identifier ((name string))
      (production :identifier ($identifier) identifier-identifier (name (name $identifier)))
      (production :identifier (get) identifier-get (name "get"))
      (production :identifier (set) identifier-set (name "set"))
      (production :identifier (exclude) identifier-exclude (name "exclude"))
      (production :identifier (include) identifier-include (name "include"))
      (production :identifier (named) identifier-named (name "named")))
    (%print-actions)
    
    (rule :qualifier ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) namespace)))
      (production :qualifier (:identifier) qualifier-identifier
        ((verify (s :unused)) (todo))
        ((eval e)
         (const a object (read-reference (lookup-variable e (name :identifier) true)))
         (rwhen (not-in a namespace :narrow-false) (throw type-error))
         (return a)))
      (production :qualifier (public) qualifier-public
        ((verify (s :unused)))
        ((eval (e :unused)) (return public-namespace)))
      (production :qualifier (private) qualifier-private
        ((verify s)
         (rwhen (not (inside-class s))
           (throw syntax-error)))
        ((eval e)
         (const q class-opt (& enclosing-class e))
         (rwhen (in q null :narrow-false) (bottom))
         (return (& private-namespace q)))))
    
    (rule :simple-qualified-identifier ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier
        ((verify (s :unused)))
        ((eval e) (return (lookup-variable e (name :identifier) false))))
      (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier
        (verify (verify :qualifier))
        ((eval e)
         (const q namespace ((eval :qualifier) e))
         (return (lookup-qualified-variable e q (name :identifier))))))
    
    (rule :expression-qualified-identifier ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :expression-qualified-identifier (:parenthesized-expression \:\: :identifier) expression-qualified-identifier-identifier
        ((verify s)
         ((verify :parenthesized-expression) s)
         (todo))
        ((eval e)
         (const a object (read-reference ((eval :parenthesized-expression) e)))
         (rwhen (not-in a namespace :narrow-false) (throw type-error))
         (return (lookup-qualified-variable e a (name :identifier))))))
    
    (rule :qualified-identifier ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple
        (verify (verify :simple-qualified-identifier))
        (eval (eval :simple-qualified-identifier)))
      (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression
        (verify (verify :expression-qualified-identifier))
        (eval (eval :expression-qualified-identifier))))
    (%print-actions)
    
    
    (%subsection "Unit Expressions")
    (rule :unit-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :unit-expression (:parenthesized-list-expression) unit-expression-parenthesized-list-expression
        (verify (verify :parenthesized-list-expression))
        (eval (eval :parenthesized-list-expression)))
      (production :unit-expression ($number :no-line-break $string) unit-expression-number-with-unit
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :unit-expression (:unit-expression :no-line-break $string) unit-expression-unit-expression-with-unit
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions)
    
    (%subsection "Primary Expressions")
    (rule :primary-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :primary-expression (null) primary-expression-null
        ((verify (s :unused)))
        ((eval (e :unused)) (return null)))
      (production :primary-expression (true) primary-expression-true
        ((verify (s :unused)))
        ((eval (e :unused)) (return true)))
      (production :primary-expression (false) primary-expression-false
        ((verify (s :unused)))
        ((eval (e :unused)) (return false)))
      (production :primary-expression (public) primary-expression-public
        ((verify (s :unused)))
        ((eval (e :unused)) (return public-namespace)))
      (production :primary-expression ($number) primary-expression-number
        ((verify (s :unused)))
        ((eval (e :unused)) (return (eval $number))))
      (production :primary-expression ($string) primary-expression-string
        ((verify (s :unused)))
        ((eval (e :unused)) (return (eval $string))))
      (production :primary-expression (this) primary-expression-this
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:unit-expression) primary-expression-unit-expression
        (verify (verify :unit-expression))
        (eval (eval :unit-expression)))
      (production :primary-expression (:array-literal) primary-expression-array-literal
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:object-literal) primary-expression-object-literal
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:function-expression) primary-expression-function-expression
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions)
    
    (rule :parenthesized-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :parenthesized-expression (\( (:assignment-expression allow-in) \)) parenthesized-expression-assignment-expression
        (verify (verify :assignment-expression))
        (eval (eval :assignment-expression))))
    
    (rule :parenthesized-list-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :parenthesized-list-expression (:parenthesized-expression) parenthesized-list-expression-parenthesized-expression
        (verify (verify :parenthesized-expression))
        (eval (eval :parenthesized-expression)))
      (production :parenthesized-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) parenthesized-list-expression-list-expression
        ((verify s)
         ((verify :list-expression) s)
         ((verify :assignment-expression) s))
        ((eval e)
         (exec (read-reference ((eval :list-expression) e)))
         (return ((eval :assignment-expression) e)))))
    (%print-actions)
    
    
    (%subsection "Function Expressions")
    (production :function-expression (function :function-signature :block) function-expression-anonymous)
    (production :function-expression (function :identifier :function-signature :block) function-expression-named)
    
    
    (%subsection "Object Literals")
    (production :object-literal (\{ \}) object-literal-empty)
    (production :object-literal (\{ :field-list \}) object-literal-list)
    
    (production :field-list (:literal-field) field-list-one)
    (production :field-list (:field-list \, :literal-field) field-list-more)
    
    (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression)
    
    (production :field-name (:identifier) field-name-identifier)
    (production :field-name ($string) field-name-string)
    (production :field-name ($number) field-name-number)
    (? js2
      (production :field-name (:parenthesized-expression) field-name-parenthesized-expression))
    (%print-actions)
    
    
    (%subsection "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    (%print-actions)
    
    
    (%subsection "Super Operator")
    (rule :super-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :super-expression (super) super-expression-super
        ((verify s)
         (rwhen (not (inside-class s))
           (throw syntax-error)))
        ((eval (e :unused)) (todo))
        ((super e) (return (lexical-class e))))
      (production :super-expression (:full-super-expression) super-expression-full-super-expression
        ((verify s)
         (rwhen (not (inside-class s))
           (throw syntax-error))
         (todo))
        ((eval (e :unused)) (todo))
        ((super e) (return (lexical-class e)))))
    
    (production :full-super-expression (super :parenthesized-expression) full-super-expression-super-parenthesized-expression)
    (%print-actions)
    
    
    (%subsection "Postfix Operators")
    (rule :postfix-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions)
    
    (rule :postfix-expression-or-super ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :postfix-expression-or-super (:postfix-expression) postfix-expression-or-super-postfix-expression
        (verify (verify :postfix-expression))
        (eval (eval :postfix-expression))
        ((super (e :unused)) (return null)))
      (production :postfix-expression-or-super (:super-expression) postfix-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    ;(rule :attribute-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier)
    (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator)
    (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call)
    
    ;(rule :full-postfix-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression)
    (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier)
    (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression)
    (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator)
    (production :full-postfix-expression (:super-expression :dot-operator) full-postfix-expression-super-dot-operator)
    (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call)
    (production :full-postfix-expression (:full-super-expression :arguments) full-postfix-expression-super-call)
    (production :full-postfix-expression (:postfix-expression-or-super :no-line-break ++) full-postfix-expression-increment)
    (production :full-postfix-expression (:postfix-expression-or-super :no-line-break --) full-postfix-expression-decrement)
    
    ;(rule :full-new-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new)
    (production :full-new-expression (new :full-super-expression :arguments) full-new-expression-super-new)
    
    ;(rule :full-new-subexpression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression)
    (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier)
    (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression)
    (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator)
    (production :full-new-subexpression (:super-expression :dot-operator) full-new-subexpression-super-dot-operator)
    
    ;(rule :short-new-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :short-new-expression (new :short-new-subexpression) short-new-expression-new)
    (production :short-new-expression (new :super-expression) short-new-expression-super-new)
    
    ;(rule :short-new-subexpression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full)
    (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short)
    (%print-actions)
    
    
    ;(rule :member-operator ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :member-operator (:dot-operator) member-operator-dot-operator)
    (production :member-operator (\. class) member-operator-class)
    (production :member-operator (\. :parenthesized-expression) member-operator-indirect)
    
    ;(rule :dot-operator ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :dot-operator (\. :qualified-identifier) dot-operator-qualified-identifier)
    (production :dot-operator (:brackets) dot-operator-brackets)
    
    ;(rule :brackets ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :brackets ([ ]) brackets-none)
    (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed)
    (production :brackets ([ :named-argument-list ]) brackets-named)
    
    ;(rule :arguments ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :arguments (:parenthesized-expressions) arguments-parenthesized-expressions)
    (production :arguments (\( :named-argument-list \)) arguments-named)
    
    ;(rule :parenthesized-expressions ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :parenthesized-expressions (\( \)) parenthesized-expressions-none)
    (production :parenthesized-expressions (:parenthesized-list-expression) parenthesized-expressions-some)
    
    ;(rule :named-argument-list ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
    (production :named-argument-list (:literal-field) named-argument-list-one)
    (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed)
    (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more)
    (%print-actions)
    
    
    (%subsection "Unary Operators")
    (rule :unary-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :unary-expression (:postfix-expression) unary-expression-postfix
        (verify (verify :postfix-expression))
        (eval (eval :postfix-expression)))
      (production :unary-expression (delete :postfix-expression-or-super) unary-expression-delete
        (verify (verify :postfix-expression-or-super))
        ((eval (e :unused)) (todo)))
      (production :unary-expression (void :unary-expression) unary-expression-void
        (verify (verify :unary-expression))
        ((eval e)
         (exec (read-reference ((eval :unary-expression) e)))
         (return undefined)))
      (production :unary-expression (typeof :unary-expression) unary-expression-typeof
        (verify (verify :unary-expression))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression) e)))
         (case a
           (:select undefined (return "undefined"))
           (:select null (return "object"))
           (:select boolean (return "boolean"))
           (:select float64 (return "number"))
           (:select string (return "string"))
           (:select namespace (return "namespace"))
           (:select attribute (return "attribute"))
           (:select (union class method-closure) (return "function"))
           (:narrow instance (return (& typeof-string a))))))
      (production :unary-expression (++ :postfix-expression-or-super) unary-expression-increment
        (verify (verify :postfix-expression-or-super))
        ((eval e)
         (const r reference ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch increment-table sa null a (vector-of object) (list-set-of named-argument)))
         (write-reference r b)
         (return b)))
      (production :unary-expression (-- :postfix-expression-or-super) unary-expression-decrement
        (verify (verify :postfix-expression-or-super))
        ((eval e)
         (const r reference ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch decrement-table sa null a (vector-of object) (list-set-of named-argument)))
         (write-reference r b)
         (return b)))
      (production :unary-expression (+ :unary-expression-or-super) unary-expression-plus
        (verify (verify :unary-expression-or-super))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch plus-table sa null a (vector-of object) (list-set-of named-argument)))))
      (production :unary-expression (- :unary-expression-or-super) unary-expression-minus
        (verify (verify :unary-expression-or-super))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch minus-table sa null a (vector-of object) (list-set-of named-argument)))))
      (production :unary-expression (~ :unary-expression-or-super) unary-expression-bitwise-not
        (verify (verify :unary-expression-or-super))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch bitwise-not-table sa null a (vector-of object) (list-set-of named-argument)))))
      (production :unary-expression (! :unary-expression) unary-expression-logical-not
        (verify (verify :unary-expression))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression) e)))
         (return (unary-not a)))))
    
    (rule :unary-expression-or-super ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :unary-expression-or-super (:unary-expression) unary-expression-or-super-unary-expression
        (verify (verify :unary-expression))
        (eval (eval :unary-expression))
        ((super (e :unused)) (return null)))
      (production :unary-expression-or-super (:super-expression) unary-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Multiplicative Operators")
    (rule :multiplicative-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        (verify (verify :unary-expression))
        (eval (eval :unary-expression)))
      (production :multiplicative-expression (:multiplicative-expression-or-super * :unary-expression-or-super) multiplicative-expression-multiply
        ((verify s)
         ((verify :multiplicative-expression-or-super) s)
         ((verify :unary-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch multiply-table sa sb a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super / :unary-expression-or-super) multiplicative-expression-divide
        ((verify s)
         ((verify :multiplicative-expression-or-super) s)
         ((verify :unary-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch divide-table sa sb a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super % :unary-expression-or-super) multiplicative-expression-remainder
        ((verify s)
         ((verify :multiplicative-expression-or-super) s)
         ((verify :unary-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch remainder-table sa sb a b)))))
    (%print-actions)
    
    (rule :multiplicative-expression-or-super ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :multiplicative-expression-or-super (:multiplicative-expression) multiplicative-expression-or-super-multiplicative-expression
        (verify (verify :multiplicative-expression))
        (eval (eval :multiplicative-expression))
        ((super (e :unused)) (return null)))
      (production :multiplicative-expression-or-super (:super-expression) multiplicative-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Additive Operators")
    (rule :additive-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        (verify (verify :multiplicative-expression))
        (eval (eval :multiplicative-expression)))
      (production :additive-expression (:additive-expression-or-super + :multiplicative-expression-or-super) additive-expression-add
        ((verify s)
         ((verify :additive-expression-or-super) s)
         ((verify :multiplicative-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :additive-expression-or-super) e)))
         (const b object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const sa class-opt ((super :additive-expression-or-super) e))
         (const sb class-opt ((super :multiplicative-expression-or-super) e))
         (return (binary-dispatch add-table sa sb a b))))
      (production :additive-expression (:additive-expression-or-super - :multiplicative-expression-or-super) additive-expression-subtract
        ((verify s)
         ((verify :additive-expression-or-super) s)
         ((verify :multiplicative-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :additive-expression-or-super) e)))
         (const b object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const sa class-opt ((super :additive-expression-or-super) e))
         (const sb class-opt ((super :multiplicative-expression-or-super) e))
         (return (binary-dispatch subtract-table sa sb a b)))))
    (%print-actions)
    
    (rule :additive-expression-or-super ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :additive-expression-or-super (:additive-expression) additive-expression-or-super-additive-expression
        (verify (verify :additive-expression))
        (eval (eval :additive-expression))
        ((super (e :unused)) (return null)))
      (production :additive-expression-or-super (:super-expression) additive-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Bitwise Shift Operators")
    (rule :shift-expression ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        (verify (verify :additive-expression))
        (eval (eval :additive-expression)))
      (production :shift-expression (:shift-expression-or-super << :additive-expression-or-super) shift-expression-left
        ((verify s)
         ((verify :shift-expression-or-super) s)
         ((verify :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-left-table sa sb a b))))
      (production :shift-expression (:shift-expression-or-super >> :additive-expression-or-super) shift-expression-right-signed
        ((verify s)
         ((verify :shift-expression-or-super) s)
         ((verify :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-right-table sa sb a b))))
      (production :shift-expression (:shift-expression-or-super >>> :additive-expression-or-super) shift-expression-right-unsigned
        ((verify s)
         ((verify :shift-expression-or-super) s)
         ((verify :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-right-unsigned-table sa sb a b)))))
    (%print-actions)
    
    (rule :shift-expression-or-super ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :shift-expression-or-super (:shift-expression) shift-expression-or-super-shift-expression
        (verify (verify :shift-expression))
        (eval (eval :shift-expression))
        ((super (e :unused)) (return null)))
      (production :shift-expression-or-super (:super-expression) shift-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Relational Operators")
    (rule (:relational-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
        (verify (verify :shift-expression))
        (eval (eval :shift-expression)))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) < :shift-expression-or-super) relational-expression-less
        ((verify s)
         ((verify :relational-expression-or-super) s)
         ((verify :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-table sa sb a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) > :shift-expression-or-super) relational-expression-greater
        ((verify s)
         ((verify :relational-expression-or-super) s)
         ((verify :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-table sb sa b a))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) <= :shift-expression-or-super) relational-expression-less-or-equal
        ((verify s)
         ((verify :relational-expression-or-super) s)
         ((verify :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-or-equal-table sa sb a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) >= :shift-expression-or-super) relational-expression-greater-or-equal
        ((verify s)
         ((verify :relational-expression-or-super) s)
         ((verify :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-or-equal-table sb sa b a))))
      (production (:relational-expression :beta) ((:relational-expression :beta) is :shift-expression) relational-expression-is
        ((verify s)
         ((verify :relational-expression) s)
         ((verify :shift-expression) s))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((verify s)
         ((verify :relational-expression) s)
         ((verify :shift-expression) s))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression-or-super) relational-expression-in
        ((verify s)
         ((verify :relational-expression) s)
         ((verify :shift-expression-or-super) s))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((verify s)
         ((verify :relational-expression) s)
         ((verify :shift-expression) s))
        ((eval (e :unused)) (todo))))
    (%print-actions)
    
    (rule (:relational-expression-or-super :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:relational-expression-or-super :beta) ((:relational-expression :beta)) relational-expression-or-super-relational-expression
        (verify (verify :relational-expression))
        (eval (eval :relational-expression))
        ((super (e :unused)) (return null)))
      (production (:relational-expression-or-super :beta) (:super-expression) relational-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Equality Operators")
    (rule (:equality-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        (verify (verify :relational-expression))
        (eval (eval :relational-expression)))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) == (:relational-expression-or-super :beta)) equality-expression-equal
        ((verify s)
         ((verify :equality-expression-or-super) s)
         ((verify :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (binary-dispatch equal-table sa sb a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) != (:relational-expression-or-super :beta)) equality-expression-not-equal
        ((verify s)
         ((verify :equality-expression-or-super) s)
         ((verify :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not (binary-dispatch equal-table sa sb a b)))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) === (:relational-expression-or-super :beta)) equality-expression-strict-equal
        ((verify s)
         ((verify :equality-expression-or-super) s)
         ((verify :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (binary-dispatch strict-equal-table sa sb a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) !== (:relational-expression-or-super :beta)) equality-expression-strict-not-equal
        ((verify s)
         ((verify :equality-expression-or-super) s)
         ((verify :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not (binary-dispatch strict-equal-table sa sb a b))))))
    (%print-actions)
    
    (rule (:equality-expression-or-super :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:equality-expression-or-super :beta) ((:equality-expression :beta)) equality-expression-or-super-equality-expression
        (verify (verify :equality-expression))
        (eval (eval :equality-expression))
        ((super (e :unused)) (return null)))
      (production (:equality-expression-or-super :beta) (:super-expression) equality-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        (verify (verify :equality-expression))
        (eval (eval :equality-expression)))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression-or-super :beta) & (:equality-expression-or-super :beta)) bitwise-and-expression-and
        ((verify s)
         ((verify :bitwise-and-expression-or-super) s)
         ((verify :equality-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-and-expression-or-super) e)))
         (const b object (read-reference ((eval :equality-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-and-expression-or-super) e))
         (const sb class-opt ((super :equality-expression-or-super) e))
         (return (binary-dispatch bitwise-and-table sa sb a b)))))
    
    (rule (:bitwise-xor-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        (verify (verify :bitwise-and-expression))
        (eval (eval :bitwise-and-expression)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression-or-super :beta) ^ (:bitwise-and-expression-or-super :beta)) bitwise-xor-expression-xor
        ((verify s)
         ((verify :bitwise-xor-expression-or-super) s)
         ((verify :bitwise-and-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-xor-expression-or-super) e)))
         (const b object (read-reference ((eval :bitwise-and-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-xor-expression-or-super) e))
         (const sb class-opt ((super :bitwise-and-expression-or-super) e))
         (return (binary-dispatch bitwise-xor-table sa sb a b)))))
    
    (rule (:bitwise-or-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        (verify (verify :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression)))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression-or-super :beta) \| (:bitwise-xor-expression-or-super :beta)) bitwise-or-expression-or
        ((verify s)
         ((verify :bitwise-or-expression-or-super) s)
         ((verify :bitwise-xor-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-or-expression-or-super) e)))
         (const b object (read-reference ((eval :bitwise-xor-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-or-expression-or-super) e))
         (const sb class-opt ((super :bitwise-xor-expression-or-super) e))
         (return (binary-dispatch bitwise-or-table sa sb a b)))))
    (%print-actions)
    
    
    (rule (:bitwise-and-expression-or-super :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-and-expression-or-super :beta) ((:bitwise-and-expression :beta)) bitwise-and-expression-or-super-bitwise-and-expression
        (verify (verify :bitwise-and-expression))
        (eval (eval :bitwise-and-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-and-expression-or-super :beta) (:super-expression) bitwise-and-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-xor-expression-or-super :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-xor-expression-or-super :beta) ((:bitwise-xor-expression :beta)) bitwise-xor-expression-or-super-bitwise-xor-expression
        (verify (verify :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-xor-expression-or-super :beta) (:super-expression) bitwise-xor-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-or-expression-or-super :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-or-expression-or-super :beta) ((:bitwise-or-expression :beta)) bitwise-or-expression-or-super-bitwise-or-expression
        (verify (verify :bitwise-or-expression))
        (eval (eval :bitwise-or-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-or-expression-or-super :beta) (:super-expression) bitwise-or-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Binary Logical Operators")
    (rule (:logical-and-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        (verify (verify :bitwise-or-expression))
        (eval (eval :bitwise-or-expression)))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((verify s)
         ((verify :logical-and-expression) s)
         ((verify :bitwise-or-expression) s))
        ((eval e)
         (const a object (read-reference ((eval :logical-and-expression) e)))
         (if (to-boolean a)
           (return (read-reference ((eval :bitwise-or-expression) e)))
           (return a)))))
    (%print-actions)
    
    (rule (:logical-xor-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        (verify (verify :logical-and-expression))
        (eval (eval :logical-and-expression)))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((verify s)
         ((verify :logical-xor-expression) s)
         ((verify :logical-and-expression) s))
        ((eval e)
         (const a object (read-reference ((eval :logical-xor-expression) e)))
         (const b object (read-reference ((eval :logical-and-expression) e)))
         (const ab boolean (to-boolean a))
         (const bb boolean (to-boolean b))
         (return (xor ab bb)))))
    (%print-actions)
    
    (rule (:logical-or-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        (verify (verify :logical-xor-expression))
        (eval (eval :logical-xor-expression)))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((verify s)
         ((verify :logical-or-expression) s)
         ((verify :logical-xor-expression) s))
        ((eval e)
         (const a object (read-reference ((eval :logical-or-expression) e)))
         (if (to-boolean a) 
           (return a)
           (return (read-reference ((eval :logical-xor-expression) e)))))))
    (%print-actions)
    
    
    (%subsection "Conditional Operator")
    (rule (:conditional-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        (verify (verify :logical-or-expression))
        (eval (eval :logical-or-expression)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((verify s)
         ((verify :logical-or-expression) s)
         ((verify :assignment-expression 1) s)
         ((verify :assignment-expression 2) s))
        ((eval e)
         (if (to-boolean (read-reference ((eval :logical-or-expression) e)))
           (return ((eval :assignment-expression 1) e))
           (return ((eval :assignment-expression 2) e))))))
    (%print-actions)
    
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or)
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional)
    (%print-actions)
    
    
    (%subsection "Assignment Operators")
    (rule (:assignment-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        (verify (verify :conditional-expression))
        (eval (eval :conditional-expression)))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((verify s)
         ((verify :postfix-expression) s)
         ((verify :assignment-expression) s))
        ((eval e)
         (const r reference ((eval :postfix-expression) e))
         (const a object (read-reference ((eval :assignment-expression) e)))
         (write-reference r a)
         (return a)))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((verify s)
         ((verify :postfix-expression-or-super) s)
         ((verify :assignment-expression) s))
        ((eval e)
         (return (eval-assignment-op (table :compound-assignment) ((super :postfix-expression-or-super) e) null
                                     (eval :postfix-expression-or-super) (eval :assignment-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment :super-expression) assignment-expression-compound-super
        ((verify s)
         ((verify :postfix-expression-or-super) s)
         ((verify :super-expression) s))
        ((eval e)
         (return (eval-assignment-op (table :compound-assignment) ((super :postfix-expression-or-super) e) ((super :super-expression) e)
                                     (eval :postfix-expression-or-super) (eval :super-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((verify s)
         ((verify :postfix-expression) s)
         ((verify :assignment-expression) s))
        ((eval (e :unused)) (todo))))
    
    (define (eval-assignment-op (table binary-table) (left-limit class-opt) (right-limit class-opt)
                                (left-eval (-> (dynamic-env) reference)) (right-eval (-> (dynamic-env) reference)) (e dynamic-env)) reference
      (const r-left reference (left-eval e))
      (const v-left object (read-reference r-left))
      (const v-right object (read-reference (right-eval e)))
      (const result object (binary-dispatch table left-limit right-limit v-left v-right))
      (write-reference r-left result)
      (return result))
    (%print-actions)
    
    (rule :compound-assignment ((table binary-table))
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
    (%print-actions)
    
    (production :logical-assignment (&&=) logical-assignment-logical-and)
    (production :logical-assignment (^^=) logical-assignment-logical-xor)
    (production :logical-assignment (\|\|=) logical-assignment-logical-or)
    (%print-actions)
    
    
    (%subsection "Comma Expressions")
    (rule (:list-expression :beta) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        (verify (verify :assignment-expression))
        (eval (eval :assignment-expression)))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((verify s)
         ((verify :list-expression) s)
         ((verify :assignment-expression) s))
        ((eval e)
         (exec (read-reference ((eval :list-expression) e)))
         (return ((eval :assignment-expression) e)))))
    
    (production :optional-expression ((:list-expression allow-in)) optional-expression-expression)
    (production :optional-expression () optional-expression-empty)
    (%print-actions)
    
    
    (%subsection "Type Expressions")
    (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression)
    (%print-actions)
    
    
    (%section "Statements")
    
    (grammar-argument :omega
                      abbrev              ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      abbrev-no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)               ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (rule (:statement :omega) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:statement :omega) (:empty-statement) statement-empty-statement
        ((verify (s :unused)))
        ((eval (e :unused) d) (return d)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        (verify (verify :expression-statement))
        ((eval e (d :unused)) (return ((eval :expression-statement) e))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:annotated-block) statement-annotated-block
        (verify (verify :annotated-block))
        (eval (eval :annotated-block)))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        (verify (verify :labeled-statement))
        (eval (eval :labeled-statement)))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        (verify (verify :if-statement))
        (eval (eval :if-statement)))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        (verify (verify :continue-statement))
        (eval (eval :continue-statement)))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        (verify (verify :break-statement))
        (eval (eval :break-statement)))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        (verify (verify :return-statement))
        ((eval e (d :unused)) (return ((eval :return-statement) e))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    
    (rule (:substatement :omega) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        (verify (verify :statement))
        (eval (eval :statement)))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    (%print-actions)
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon abbrev-no-short-if) () semicolon-abbrev-no-short-if)
    
    
    (%subsection "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%subsection "Expression Statement")
    (rule :expression-statement ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        (verify (verify :list-expression))
        ((eval e) (return (read-reference ((eval :list-expression) e))))))
    (%print-actions)
    
    
    (%subsection "Super Statement")
    (production :super-statement (super :arguments) super-statement-super-arguments)
    (%print-actions)
    
    
    (%subsection "Block Statement")
    (rule :annotated-block ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production :annotated-block (:attributes :block) annotated-block-attributes-and-block
        (verify (verify :block)) ;******
        (eval (eval :block)))) ;******
    
    (rule :block ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production :block ({ :directives }) block-directives
        (verify (verify :directives))
        (eval (eval :directives))))
    
    (rule :directives ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production :directives () directives-none
        ((verify (s :unused)))
        ((eval (e :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((verify s)
         ((verify :directives-prefix) s)
         ((verify :directive) s))
        ((eval e d)
         (const v object ((eval :directive) e d))
         (return ((eval :directives-prefix) e v)))))
    
    (rule :directives-prefix ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production :directives-prefix () directives-prefix-none
        ((verify (s :unused)))
        ((eval (e :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((verify s)
         ((verify :directives-prefix) s)
         ((verify :directive) s))
        ((eval e d)
         (const v object ((eval :directive) e d))
         (return ((eval :directives-prefix) e v)))))
    (%print-actions)
    
    
    (%subsection "Labeled Statements")
    (rule (:labeled-statement :omega) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((verify s) ((verify :substatement) (add-label s (name :identifier))))
        ((eval e d)
         (catch ((return ((eval :substatement) e d)))
           (x) (if (and (in x go-break :narrow-true) (= (& label x) (name :identifier) string))
                 (return (& value x))
                 (throw x))))))
    (%print-actions)
    
    
    (%subsection "If Statement")
    (rule (:if-statement :omega) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:if-statement abbrev) (if :parenthesized-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((verify s)
         ((verify :parenthesized-list-expression) s)
         ((verify :substatement) s))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :parenthesized-list-expression) e)))
           (return ((eval :substatement) e d))
           (return d))))
      (production (:if-statement full) (if :parenthesized-list-expression (:substatement full)) if-statement-if-then-full
        ((verify s)
         ((verify :parenthesized-list-expression) s)
         ((verify :substatement) s))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :parenthesized-list-expression) e)))
           (return ((eval :substatement) e d))
           (return d))))
      (production (:if-statement :omega) (if :parenthesized-list-expression (:substatement abbrev-no-short-if)
                                             else (:substatement :omega)) if-statement-if-then-else
        ((verify s)
         ((verify :parenthesized-list-expression) s)
         ((verify :substatement 1) s)
         ((verify :substatement 2) s))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :parenthesized-list-expression) e)))
           (return ((eval :substatement 1) e d))
           (return ((eval :substatement 2) e d))))))
    (%print-actions)
    
    
    (%subsection "Switch Statement")
    (production :switch-statement (switch :parenthesized-list-expression { :case-statements }) switch-statement-cases)
    
    (production :case-statements () case-statements-none)
    (production :case-statements (:case-label) case-statements-one)
    (production :case-statements (:case-label :case-statements-prefix (:case-statement abbrev)) case-statements-more)
    
    (production :case-statements-prefix () case-statements-prefix-none)
    (production :case-statements-prefix (:case-statements-prefix (:case-statement full)) case-statements-prefix-more)
    
    (production (:case-statement :omega_2) ((:substatement :omega_2)) case-statement-statement)
    (production (:case-statement :omega_2) (:case-label) case-statement-case-label)
    
    (production :case-label (case (:list-expression allow-in) \:) case-label-case)
    (production :case-label (default \:) case-label-default)
    (%print-actions)
    
    
    (%subsection "Do-While Statement")
    (production :do-statement (do (:substatement abbrev) while :parenthesized-list-expression) do-statement-do-while)
    (%print-actions)
    
    
    (%subsection "While Statement")
    (production (:while-statement :omega) (while :parenthesized-list-expression (:substatement :omega)) while-statement-while)
    (%print-actions)
    
    
    (%subsection "For Statements")
    (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                             (:substatement :omega)) for-statement-c-style)
    (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:substatement :omega)) for-statement-in)
    
    (production :for-initializer () for-initializer-empty)
    (production :for-initializer ((:list-expression no-in)) for-initializer-expression)
    (production :for-initializer (:attributes :variable-definition-kind (:variable-binding-list no-in)) for-initializer-variable-definition)
    
    (production :for-in-binding (:postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:attributes :variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    (%print-actions)
    
    
    (%subsection "With Statement")
    (production (:with-statement :omega) (with :parenthesized-list-expression (:substatement :omega)) with-statement-with)
    (%print-actions)
    
    
    (%subsection "Continue and Break Statements")
    (rule :continue-statement ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-continue d ""))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-continue d (name :identifier))))))
    
    (rule :break-statement ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-break d ""))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-break d (name :identifier))))))
    (%print-actions)
    
    
    (%subsection "Return Statement")
    (rule :return-statement ((verify (-> (verify-env) void)) (eval (-> (dynamic-env) object)))
      (production :return-statement (return) return-statement-default
        ((verify s)
         (when (not (& can-return s))
           (throw syntax-error)))
        ((eval (e :unused)) (throw (new go-return undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((verify s)
         (when (not (& can-return s))
           (throw syntax-error))
         ((verify :list-expression) s))
        ((eval e) (throw (new go-return (read-reference ((eval :list-expression) e)))))))
    (%print-actions)
    
    
    (%subsection "Throw Statement")
    (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw)
    (%print-actions)
    
    
    (%subsection "Try Statement")
    (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses)
    (production :try-statement (try :block :finally-clause) try-statement-finally-clause)
    (production :try-statement (try :block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
    
    (production :catch-clauses (:catch-clause) catch-clauses-one)
    (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
    
    (production :catch-clause (catch \( :parameter \) :block) catch-clause-block)
    
    (production :finally-clause (finally :block) finally-clause-block)
    (%print-actions)
    
    
    (%section "Directives")
    (rule (:directive :omega_2) ((verify (-> (verify-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        (verify (verify :statement))
        (eval (eval :statement)))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:directive :omega_2) (:attribute :no-line-break :attributes (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:directive :omega_2) (:package-definition) directive-package-definition
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
    (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((verify (s :unused)) (todo))
          ((eval (e :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((verify (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    (%print-actions)
    
    (production (:annotatable-directive :omega_2) (:export-definition (:semicolon :omega_2)) annotatable-directive-export-definition)
    (production (:annotatable-directive :omega_2) (:variable-definition (:semicolon :omega_2)) annotatable-directive-variable-definition)
    (production (:annotatable-directive :omega_2) ((:function-definition :omega_2)) annotatable-directive-function-definition)
    (production (:annotatable-directive :omega_2) ((:class-definition :omega_2)) annotatable-directive-class-definition)
    (production (:annotatable-directive :omega_2) (:namespace-definition (:semicolon :omega_2)) annotatable-directive-namespace-definition)
    (? js2
      (production (:annotatable-directive :omega_2) ((:interface-definition :omega_2)) annotatable-directive-interface-definition))
    (production (:annotatable-directive :omega_2) (:use-directive (:semicolon :omega_2)) annotatable-directive-use-directive)
    (production (:annotatable-directive :omega_2) (:import-directive (:semicolon :omega_2)) annotatable-directive-import-directive)
    (%print-actions)
    
    
    (%subsection "Attributes")
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
    (%print-actions)
    
    

    (%subsection "Use Directive")
    (production :use-directive (use namespace :parenthesized-list-expression :includes-excludes) use-directive-normal)
    
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
    (production :qualified-wildcard-pattern (:parenthesized-expression \:\: :wildcard-pattern) qualified-wildcard-pattern-expression-qualifier)
    
    (production :wildcard-pattern (*) wildcard-pattern-all)
    (production :wildcard-pattern ($regular-expression) wildcard-pattern-regular-expression)
    |#
    
    
    (%subsection "Import Directive")
    (production :import-directive (import :import-binding :includes-excludes) import-directive-import)
    (production :import-directive (import :import-binding \, namespace :parenthesized-list-expression :includes-excludes)
                import-directive-import-namespaces)
    
    (production :import-binding (:import-source) import-binding-import-source)
    (production :import-binding (:identifier = :import-source) import-binding-named-import-source)
    
    (production :import-source ($string) import-source-string)
    (production :import-source (:package-name) import-source-package-name)
    
    
    (? js2
      (%subsection "Include Directive")
      (production :include-directive (include :no-line-break $string) include-directive-include))
    
    
    (%subsection "Pragma")
    (production :pragma (use :pragma-items) pragma-pragma-items)
    
    (production :pragma-items (:pragma-item) pragma-items-one)
    (production :pragma-items (:pragma-items \, :pragma-item) pragma-items-more)
    
    (production :pragma-item (:pragma-expr) pragma-item-pragma-expr)
    (production :pragma-item (:pragma-expr \?) pragma-item-optional-pragma-expr)
    
    (production :pragma-expr (:identifier) pragma-expr-identifier)
    (production :pragma-expr (:identifier :parenthesized-list-expression) pragma-expr-identifier-and-arguments)
    
    
    (%section "Definitions")
    (%subsection "Export Definition")
    (production :export-definition (export :export-binding-list) export-definition-definition)
    
    (production :export-binding-list (:export-binding) export-binding-list-one)
    (production :export-binding-list (:export-binding-list \, :export-binding) export-binding-list-more)
    
    (production :export-binding (:function-name) export-binding-simple)
    (production :export-binding (:function-name = :function-name) export-binding-initializer)
    
    
    (%subsection "Variable Definition")
    (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition)
    
    (production :variable-definition-kind (var) variable-definition-kind-var)
    (production :variable-definition-kind (const) variable-definition-kind-const)
    
    (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one)
    (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more)
    
    (production (:variable-binding :beta) ((:typed-identifier :beta)) variable-binding-simple)
    (production (:variable-binding :beta) ((:typed-identifier :beta) = (:variable-initializer :beta)) variable-binding-initialized)
    
    (production (:variable-initializer :beta) ((:assignment-expression :beta)) variable-initializer-assignment-expression)
    (production (:variable-initializer :beta) (:multiple-attributes) variable-initializer-multiple-attributes)
    (production (:variable-initializer :beta) (abstract) variable-initializer-abstract)
    (production (:variable-initializer :beta) (final) variable-initializer-final)
    (production (:variable-initializer :beta) (private) variable-initializer-private)
    (production (:variable-initializer :beta) (static) variable-initializer-static)
    
    (production :multiple-attributes (:attribute :no-line-break :attribute) multiple-attributes-two)
    (production :multiple-attributes (:multiple-attributes :no-line-break :attribute) multiple-attributes-more)
    
    (production (:typed-identifier :beta) (:identifier) typed-identifier-identifier)
    (production (:typed-identifier :beta) (:identifier \: (:type-expression :beta)) typed-identifier-identifier-and-type)
    ;(production (:typed-identifier :beta) ((:type-expression :beta) :identifier) typed-identifier-type-and-identifier)
    
    
    (production :simple-variable-definition (var :untyped-variable-binding-list) simple-variable-definition-definition)
    
    (production :untyped-variable-binding-list (:untyped-variable-binding) untyped-variable-binding-list-one)
    (production :untyped-variable-binding-list (:untyped-variable-binding-list \, :untyped-variable-binding) untyped-variable-binding-list-more)
    
    (production :untyped-variable-binding (:identifier) untyped-variable-binding-simple)
    (production :untyped-variable-binding (:identifier = (:variable-initializer allow-in)) untyped-variable-binding-initialized)
    
    
    (%subsection "Function Definition")
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
    (production :optional-parameters (:rest-parameter) optional-parameters-rest-parameter)
    
    (production :rest-parameter (\.\.\.) rest-parameter-none)
    (production :rest-parameter (\.\.\. :parameter) rest-parameter-parameter)
    
    (production :parameter ((:typed-identifier allow-in)) parameter-typed-identifier)
    (production :parameter (const (:typed-identifier allow-in)) parameter-const-typed-identifier)
    (production :parameter (named (:typed-identifier allow-in)) parameter-named-typed-identifier)
    (production :parameter (const named (:typed-identifier allow-in)) parameter-const-named-typed-identifier)
    (production :parameter (named const (:typed-identifier allow-in)) parameter-named-const-typed-identifier)
    
    (production :optional-parameter (:parameter = (:assignment-expression allow-in)) optional-parameter-assignment-expression)
    
    (production :result-signature () result-signature-none)
    (production :result-signature (\: (:type-expression allow-in)) result-signature-colon-and-type-expression)
    ;(production :result-signature ((:- {) (:type-expression allow-in)) result-signature-type-expression)
    
    
    (%subsection "Class Definition")
    (production (:class-definition :omega_2) (class :identifier :inheritance :block) class-definition-definition)
    (production (:class-definition :omega_2) (class :identifier (:semicolon :omega_2)) class-definition-declaration)
    
    (production :inheritance () inheritance-none)
    (production :inheritance (extends (:type-expression allow-in)) inheritance-extends)
    (? js2
      (production :inheritance (implements :type-expression-list) inheritance-implements)
      (production :inheritance (extends (:type-expression allow-in) implements :type-expression-list) inheritance-extends-implements)
      
      (%subsection "Interface Definition")
      (production (:interface-definition :omega_2) (interface :identifier :extends-list :block) interface-definition-definition)
      (production (:interface-definition :omega_2) (interface :identifier (:semicolon :omega_2)) interface-definition-declaration)
      
      (production :extends-list () extends-list-none)
      (production :extends-list (extends :type-expression-list) extends-list-one)
      
      (production :type-expression-list ((:type-expression allow-in)) type-expression-list-one)
      (production :type-expression-list (:type-expression-list \, (:type-expression allow-in)) type-expression-list-more))
    
    
    (%subsection "Namespace Definition")
    (production :namespace-definition (namespace :identifier) namespace-definition-normal)
    
    
    (%subsection "Package Definition")
    (production :package-definition (package :block) package-definition-anonymous)
    (production :package-definition (package :package-name :block) package-definition-named)
    
    (production :package-name (:identifier) package-name-one)
    (production :package-name (:package-name \. :identifier) package-name-more)
    
    
    (%section "Programs")
    (rule :program ((eval-program object))
      (production :program (:directives) program-directives
        (eval-program
         (begin
          ((verify :directives) initial-verify-env)
          (return ((eval :directives) initial-dynamic-env undefined))))))))


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


(defun depict-js-terminals (markup-stream grammar)
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
      (depict-paragraph (markup-stream :section-heading)
        (depict-link (markup-stream :definition "terminals" "" nil)
          (depict markup-stream "Terminals")))
      (mapc #'depict-terminal-bin '("General tokens: " "Punctuation tokens: " "Future punctuation tokens: "
                                    "Reserved words: " "Future reserved words: " "Non-reserved words: ")
            (coerce bins 'list)))))


#|
(values
 (length (grammar-states *jg*))
 (depict-rtf-to-local-file
  "JS20/ParserGrammarJS2.rtf"
  "JavaScript 2.0 Syntactic Grammar"
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *jg*)
      (depict-world-commands markup-stream *jw* :visible-semantics nil)))
 (depict-rtf-to-local-file
  "JS20/ParserSemanticsJS2.rtf"
  "JavaScript 2.0 Syntactic Semantics"
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *jg*)
      (depict-world-commands markup-stream *jw*)))
 (compute-ecma-subset)
 (depict-rtf-to-local-file
  "JS20/ParserGrammarES4.rtf"
  "ECMAScript Edition 4 Syntactic Grammar"
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *eg*)
      (depict-world-commands markup-stream *ew* :visible-semantics nil)))
 (depict-rtf-to-local-file
  "JS20/ParserSemanticsES4.rtf"
  "ECMAScript Edition 4 Syntactic Semantics"
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *eg*)
      (depict-world-commands markup-stream *ew*)))
 
 (length (grammar-states *jg*))
 (depict-html-to-local-file
  "JS20/ParserGrammarJS2.html"
  "JavaScript 2.0 Syntactic Grammar"
  t
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *jg*)
      (depict-world-commands markup-stream *jw* :visible-semantics nil))
  :external-link-base "notation.html")
 (depict-html-to-local-file
  "JS20/ParserSemanticsJS2.html"
  "JavaScript 2.0 Syntactic Semantics"
  t
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *jg*)
      (depict-world-commands markup-stream *jw*))
  :external-link-base "notation.html")
 (compute-ecma-subset)
 (depict-html-to-local-file
  "JS20/ParserGrammarES4.html"
  "ECMAScript Edition 4 Syntactic Grammar"
  t
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *eg*)
      (depict-world-commands markup-stream *ew* :visible-semantics nil))
  :external-link-base "notation.html")
 (depict-html-to-local-file
  "JS20/ParserSemanticsES4.html"
  "ECMAScript Edition 4 Syntactic Semantics"
  t
  #'(lambda (markup-stream)
      (depict-js-terminals markup-stream *eg*)
      (depict-world-commands markup-stream *ew*))
  :external-link-base "notation.html"))


(depict-rtf-to-local-file
 "JS20/ParserSemanticsJS2.rtf"
 "JavaScript 2.0 Syntactic Semantics"
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg*)
     (depict-world-commands markup-stream *jw*)))

(depict-html-to-local-file
 "JS20/ParserSemanticsJS2.html"
 "JavaScript 2.0 Syntactic Semantics"
 t
 #'(lambda (markup-stream)
     (depict-js-terminals markup-stream *jg*)
     (depict-world-commands markup-stream *jw*))
 :external-link-base "notation.html")


(with-local-output (s "JS20/ParserGrammarJS2 states") (print-grammar *jg* s))
(compute-ecma-subset)
(with-local-output (s "JS20/ParserGrammarES4 states") (print-grammar *eg* s))
|#

(length (grammar-states *jg*))
