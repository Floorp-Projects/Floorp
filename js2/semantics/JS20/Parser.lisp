;;;
;;; JavaScript 2.0 parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(defparameter *jw-source* 
  '((line-grammar code-grammar :lr-1 :program)
    
    (%section :semantics "Errors")
    
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
    
    
    (%section :semantics "Objects")
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
    (define (has-type (o object) (c class)) boolean
      (return (is-ancestor c (object-type o))))
    
    (%text :comment "Return " (:tag true) " if " (:local o) " is an instance of class " (:local c) ". Consider "
           (:tag null) " to be an instance of the classes " (:character-literal "Null") ", "
           (:character-literal "Object") ", and all other non-primitive classes.")
    (define (relaxed-has-type (o object) (c class)) boolean
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
    
    
    (%section :semantics "References")
    (deftuple qualified-name (namespace namespace) (name string))
    
    (deftuple partial-name (namespaces (list-set namespace)) (name string))
    
    (deftuple dot-reference
      (base object)
      (super class-opt)
      (prop-name partial-name))
    
    (deftuple bracket-reference
      (base object)
      (super class-opt)
      (args argument-list))
    
    (deftype reference (union object dot-reference bracket-reference))
    
    (%text :comment "Read the " (:type reference) " " (:local r) ".")
    (define (read-reference (r reference)) object
      (case r
        (:narrow object (return r))
        (:narrow dot-reference (return (read-property (& base r) (& prop-name r) (& super r))))
        (:narrow bracket-reference (return (unary-dispatch bracket-read-table (& super r) null (& base r) (& args r))))))
    
    (%text :comment "Write " (:local v) " into the " (:type reference) " " (:local r) ".")
    (define (write-reference (r reference) (v object)) void
      (case r
        (:select object (throw reference-error))
        (:narrow dot-reference (write-property (& base r) (& prop-name r) (& super r) v))
        (:narrow bracket-reference
          (const args argument-list (new argument-list (append (vector v) (& positional (& args r))) (& named (& args r))))
          (exec (unary-dispatch bracket-write-table (& super r) null (& base r) args)))))
    
    (define (delete-reference (r reference)) object
      (case r
        (:select object (throw reference-error))
        (:narrow dot-reference (return (delete-property (& base r) (& prop-name r) (& super r))))
        (:narrow bracket-reference (return (unary-dispatch bracket-delete-table (& super r) null (& base r) (& args r))))))
    
    (define (reference-base (r reference)) object
      (case r
        (:narrow object (return null))
        (:narrow (union dot-reference bracket-reference) (return (& base r)))))
    
    
    (%section :semantics "Signatures")
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
    
    
    (%section :semantics "Argument Lists")
    (deftuple named-argument (name string) (value object))
    
    (deftuple argument-list
      (positional (vector object))
      (named (list-set named-argument)))
    
    (%text :comment "The first " (:type object) " is the " (:character-literal "this") " value.")
    (deftype invoker (-> (object argument-list) object))
    
    
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
          (return ((& call (& f d)) o (new argument-list (vector-of object) (list-set-of named-argument)))))))
    
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
    
    (define (read-property (o object :unused) (prop-name partial-name :unused) (super class-opt :unused)) object
      (todo))
    ;(const ns namespace (resolve-object-namespace o name uses))
    ;(return (read-qualified-property o name ns false)))
    
    (define (write-property (o object :unused) (prop-name partial-name :unused) (super class-opt :unused) (new-value object :unused)) void
      (todo))
    
    (define (delete-property (o object :unused) (prop-name partial-name :unused) (super class-opt :unused)) boolean
      (todo))
    
    (define (write-qualified-property (o object :unused) (name string :unused) (ns namespace :unused) (indexable-only boolean :unused) (new-value object :unused)) void
      (todo))
    
    (define (delete-qualified-property (o object :unused) (name string :unused) (ns namespace :unused) (indexable-only boolean :unused)) boolean
      (todo))
    
    
    (%section :semantics "Static Constraint Environments")
    (deftuple constraint-env
      (enclosing-class class-opt)
      (labels (vector string))
      (can-return boolean)
      (constants (vector definition)))
    
    (define initial-constraint-env constraint-env (new constraint-env null (vector-of string) false (vector-of definition)))
    
    (%text :comment "Return a " (:type constraint-env) " with label " (:local label) " prepended to " (:local s) ".")
    (define (add-label (t constraint-env) (label string)) constraint-env
      (return (new constraint-env (& enclosing-class t) (append (vector label) (& labels t)) (& can-return t) (& constants t))))
    
    (%text :comment "Return " (:tag true) " if this code is inside a class body.")
    (define (inside-class (s constraint-env)) boolean
      (return (/= (& enclosing-class s) null class-opt)))
    
    
    (%section :semantics "Dynamic Environments")
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
    
    
    (define (lookup-variable (e dynamic-env :unused) (name string :unused) (internal-is-namespace boolean :unused)) reference
      (todo))
    
    (define (lookup-qualified-variable (e dynamic-env :unused) (namespace namespace :unused) (name string :unused)) reference
      (todo))
    
    (%text :comment "Return the value of " (:character-literal "this") ". Throw an exception if there is no " (:character-literal "this") " defined.")
    (define (lookup-this (e dynamic-env :unused)) object
      (todo))
    
    
    (%section :semantics "Unary Operators")
    (deftuple unary-method 
      (operand-type class)
      (op (-> (object object argument-list) object)))
    
    (defrecord unary-table
      (methods (list-set unary-method) :var))
    
    (%text :comment "Return " (:tag true) " if " (:local v) " is a member of class " (:local c) " and, if "
           (:local limit) " is non-" (:tag null) ", " (:local c) " is a proper ancestor of " (:local limit) ".")
    (define (limited-has-type (v object) (c class) (limit class-opt)) boolean
      (if (has-type v c)
        (if (in limit (tag null) :narrow-false)
          (return true)
          (return (is-proper-ancestor c limit)))
        (return false)))
    
    (%text :comment "Dispatch the unary operator described by " (:local table) " applied to the " (:character-literal "this")
           " value " (:local this) ", the first argument " (:local op)
           ", and optionally other arguments " (:local args)
           ". If " (:local limit) " is non-" (:tag null)
           ", restrict the lookup to operators defined on the proper ancestors of " (:local limit) ".")
    (define (unary-dispatch (table unary-table) (limit class-opt) (this object) (op object) (args argument-list)) object
      (const applicable-ops (list-set unary-method)
        (map (& methods table) m m (limited-has-type op (& operand-type m) limit)))
      (reserve best)
      (if (some applicable-ops best
                (every applicable-ops m2 (is-ancestor (& operand-type m2) (& operand-type best))) :define-true)
        (return ((& op best) this op args))
        (throw property-not-found-error)))
    
    
    (%subsection :semantics "Unary Operator Tables")
    
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
        (:select (union undefined null boolean float64 string namespace attribute) (throw type-error))
        (:narrow (union class instance) (return ((& call a) this args)))
        (:narrow method-closure (return (call-object (& this a) (& f (& method a)) args)))))
    
    (define (construct-object (this object) (a object) (args argument-list)) object
      (case a
        (:select (union undefined null boolean float64 string namespace attribute method-closure) (throw type-error))
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
      (return (unary-dispatch plus-table null null a (new argument-list (vector-of object) (list-set-of named-argument)))))
    
    (define (unary-not (a object)) object
      (return (not (to-boolean a))))
    
    
    (%section :semantics "Binary Operators")
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
        (map (& methods table) m m (and (limited-has-type left (& left-type m) left-limit)
                                        (limited-has-type right (& right-type m) right-limit))))
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
    
    (%subsection "Qualified Identifiers")
    (rule :qualifier ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) namespace)))
      (production :qualifier (:identifier) qualifier-identifier
        ((constrain (s :unused)) (todo))
        ((eval e)
         (const a object (read-reference (lookup-variable e (name :identifier) true)))
         (rwhen (not-in a namespace :narrow-false) (throw type-error))
         (return a)))
      (production :qualifier (public) qualifier-public
        ((constrain (s :unused)))
        ((eval (e :unused)) (return public-namespace)))
      (production :qualifier (private) qualifier-private
        ((constrain s)
         (rwhen (not (inside-class s))
           (throw syntax-error)))
        ((eval e)
         (const q class-opt (& enclosing-class e))
         (rwhen (in q null :narrow-false) (bottom))
         (return (& private-namespace q)))))
    
    (rule :simple-qualified-identifier ((constrain (-> (constraint-env) void)) (name (-> (dynamic-env) partial-name)) (eval (-> (dynamic-env) reference)))
      (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier
        ((constrain (s :unused)))
        ((name e) (return (new partial-name (dynamic-env-uses e) (name :identifier))))
        ((eval e) (return (lookup-variable e (name :identifier) false))))
      (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier
        (constrain (constrain :qualifier))
        ((name e)
         (const q namespace ((eval :qualifier) e))
         (return (new partial-name (list-set q) (name :identifier))))
        ((eval e)
         (const q namespace ((eval :qualifier) e))
         (return (lookup-qualified-variable e q (name :identifier))))))
    
    (rule :expression-qualified-identifier ((constrain (-> (constraint-env) void)) (name (-> (dynamic-env) partial-name)) (eval (-> (dynamic-env) reference)))
      (production :expression-qualified-identifier (:paren-expression \:\: :identifier) expression-qualified-identifier-identifier
        ((constrain s)
         ((constrain :paren-expression) s)
         (todo))
        ((name e)
         (const q object (read-reference ((eval :paren-expression) e)))
         (rwhen (not-in q namespace :narrow-false) (throw type-error))
         (return (new partial-name (list-set q) (name :identifier))))
        ((eval e)
         (const q object (read-reference ((eval :paren-expression) e)))
         (rwhen (not-in q namespace :narrow-false) (throw type-error))
         (return (lookup-qualified-variable e q (name :identifier))))))
    
    (rule :qualified-identifier ((constrain (-> (constraint-env) void)) (name (-> (dynamic-env) partial-name)) (eval (-> (dynamic-env) reference)))
      (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple
        (constrain (constrain :simple-qualified-identifier))
        (name (name :simple-qualified-identifier))
        (eval (eval :simple-qualified-identifier)))
      (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression
        (constrain (constrain :expression-qualified-identifier))
        (name (name :expression-qualified-identifier))
        (eval (eval :expression-qualified-identifier))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Unit Expressions")
    (rule :unit-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :unit-expression (:paren-list-expression) unit-expression-paren-list-expression
        (constrain (constrain :paren-list-expression))
        (eval (eval :paren-list-expression)))
      (production :unit-expression ($number :no-line-break $string) unit-expression-number-with-unit
        ((constrain (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :unit-expression (:unit-expression :no-line-break $string) unit-expression-unit-expression-with-unit
        ((constrain (s :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    (%subsection "Primary Expressions")
    (rule :primary-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :primary-expression (null) primary-expression-null
        ((constrain (s :unused)))
        ((eval (e :unused)) (return null)))
      (production :primary-expression (true) primary-expression-true
        ((constrain (s :unused)))
        ((eval (e :unused)) (return true)))
      (production :primary-expression (false) primary-expression-false
        ((constrain (s :unused)))
        ((eval (e :unused)) (return false)))
      (production :primary-expression (public) primary-expression-public
        ((constrain (s :unused)))
        ((eval (e :unused)) (return public-namespace)))
      (production :primary-expression ($number) primary-expression-number
        ((constrain (s :unused)))
        ((eval (e :unused)) (return (eval $number))))
      (production :primary-expression ($string) primary-expression-string
        ((constrain (s :unused)))
        ((eval (e :unused)) (return (eval $string))))
      (production :primary-expression (this) primary-expression-this
        ((constrain (s :unused)) (todo))
        (eval lookup-this))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((constrain (s :unused)))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:unit-expression) primary-expression-unit-expression
        (constrain (constrain :unit-expression))
        (eval (eval :unit-expression)))
      (production :primary-expression (:array-literal) primary-expression-array-literal
        ((constrain (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:object-literal) primary-expression-object-literal
        ((constrain (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :primary-expression (:function-expression) primary-expression-function-expression
        (constrain (constrain :function-expression))
        (eval (eval :function-expression))))
    
    (rule :paren-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :paren-expression (\( (:assignment-expression allow-in) \)) paren-expression-assignment-expression
        (constrain (constrain :assignment-expression))
        (eval (eval :assignment-expression))))
    
    (rule :paren-list-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (eval-as-list (-> (dynamic-env) (vector object))))
      (production :paren-list-expression (:paren-expression) paren-list-expression-paren-expression
        (constrain (constrain :paren-expression))
        (eval (eval :paren-expression))
        ((eval-as-list e)
         (const elt object (read-reference ((eval :paren-expression) e)))
         (return (vector elt))))
      (production :paren-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) paren-list-expression-list-expression
        ((constrain s)
         ((constrain :list-expression) s)
         ((constrain :assignment-expression) s))
        ((eval e)
         (exec (read-reference ((eval :list-expression) e)))
         (return ((eval :assignment-expression) e)))
        ((eval-as-list e)
         (const elts (vector object) ((eval-as-list :list-expression) e))
         (const elt object (read-reference ((eval :assignment-expression) e)))
         (return (append elts (vector elt))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Function Expressions")
    (rule :function-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :function-expression (function :function-signature :block) function-expression-anonymous
        ((constrain (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :function-expression (function :identifier :function-signature :block) function-expression-named
        ((constrain (s :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Object Literals")
    (production :object-literal (\{ \}) object-literal-empty)
    (production :object-literal (\{ :field-list \}) object-literal-list)
    
    (production :field-list (:literal-field) field-list-one)
    (production :field-list (:field-list \, :literal-field) field-list-more)
    
    (rule :literal-field ((constrain (-> (constraint-env) (list-set string))) (eval (-> (dynamic-env) named-argument)))
      (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression
        ((constrain s)
         (const names (list-set string) ((constrain :field-name) s))
         ((constrain :assignment-expression) s)
         (return names))
        ((eval e)
         (const name string ((eval :field-name) e))
         (const value object (read-reference ((eval :assignment-expression) e)))
         (return (new named-argument name value)))))
    
    (rule :field-name ((constrain (-> (constraint-env) (list-set string))) (eval (-> (dynamic-env) string)))
      (production :field-name (:identifier) field-name-identifier
        ((constrain (s :unused)) (return (list-set (name :identifier))))
        ((eval (e :unused)) (return (name :identifier))))
      (production :field-name ($string) field-name-string
        ((constrain (s :unused)) (return (list-set (eval $string))))
        ((eval (e :unused)) (return (eval $string))))
      (production :field-name ($number) field-name-number
        ((constrain (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (? js2
        (production :field-name (:paren-expression) field-name-paren-expression
          ((constrain (s :unused)) (todo))
          ((eval (e :unused)) (todo)))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Super Expressions")
    (rule :super-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) object)) (super (-> (dynamic-env) class)))
      (production :super-expression (super) super-expression-super
        ((constrain s)
         (rwhen (not (inside-class s))
           (throw syntax-error)))
        (eval lookup-this)
        ((super e) (return (lexical-class e))))
      (production :super-expression (:full-super-expression) super-expression-full-super-expression
        (constrain (constrain :full-super-expression))
        (eval (eval :full-super-expression))
        (super (super :full-super-expression))))
    
    (rule :full-super-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) object)) (super (-> (dynamic-env) class)))
      (production :full-super-expression (super :paren-expression) full-super-expression-super-paren-expression
        ((constrain s)
         (rwhen (not (inside-class s))
           (throw syntax-error))
         ((constrain :paren-expression) s))
        ((eval e)
         (const a object (read-reference ((eval :paren-expression) e)))
         (const c class (lexical-class e))
         (rwhen (not (has-type a c))
           (throw type-error))
         (return a))
        ((super e) (return (lexical-class e)))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Postfix Expressions")
    (rule :postfix-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        (constrain (constrain :attribute-expression))
        (eval (eval :attribute-expression)))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        (constrain (constrain :full-postfix-expression))
        (eval (eval :full-postfix-expression)))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        (constrain (constrain :short-new-expression))
        (eval (eval :short-new-expression))))
    
    (rule :postfix-expression-or-super ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :postfix-expression-or-super (:postfix-expression) postfix-expression-or-super-postfix-expression
        (constrain (constrain :postfix-expression))
        (eval (eval :postfix-expression))
        ((super (e :unused)) (return null)))
      (production :postfix-expression-or-super (:super-expression) postfix-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule :attribute-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier
        (constrain (constrain :simple-qualified-identifier))
        (eval (eval :simple-qualified-identifier)))
      (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator
        ((constrain s)
         ((constrain :attribute-expression) s)
         ((constrain :member-operator) s))
        ((eval e)
         (const a object (read-reference ((eval :attribute-expression) e)))
         (return ((eval :member-operator) e a))))
      (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call
        ((constrain s)
         ((constrain :attribute-expression) s)
         ((constrain :arguments) s))
        ((eval e)
         (const r reference ((eval :attribute-expression) e))
         (const f object (read-reference r))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch call-table null base f args)))))
    
    (rule :full-postfix-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression
        (constrain (constrain :primary-expression))
        (eval (eval :primary-expression)))
      (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier
        (constrain (constrain :expression-qualified-identifier))
        (eval (eval :expression-qualified-identifier)))
      (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression
        (constrain (constrain :full-new-expression))
        (eval (eval :full-new-expression)))
      (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator
        ((constrain s)
         ((constrain :full-postfix-expression) s)
         ((constrain :member-operator) s))
        ((eval e)
         (const a object (read-reference ((eval :full-postfix-expression) e)))
         (return ((eval :member-operator) e a))))
      (production :full-postfix-expression (:super-expression :dot-operator) full-postfix-expression-super-dot-operator
        ((constrain s)
         ((constrain :super-expression) s)
         ((constrain :dot-operator) s))
        ((eval e)
         (const a object (read-reference ((eval :super-expression) e)))
         (const sa class ((super :super-expression) e))
         (return ((eval :dot-operator) e a sa))))
      (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call
        ((constrain s)
         ((constrain :full-postfix-expression) s)
         ((constrain :arguments) s))
        ((eval e)
         (const r reference ((eval :full-postfix-expression) e))
         (const f object (read-reference r))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch call-table null base f args))))
      (production :full-postfix-expression (:full-super-expression :arguments) full-postfix-expression-super-call
        ((constrain s)
         ((constrain :full-super-expression) s)
         ((constrain :arguments) s))
        ((eval e)
         (const r reference ((eval :full-super-expression) e))
         (const f object (read-reference r))
         (const base object (reference-base r))
         (const sf class ((super :full-super-expression) e))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch call-table sf base f args))))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break ++) full-postfix-expression-increment
        (constrain (constrain :postfix-expression-or-super))
        ((eval e)
         (const r reference ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch increment-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return a)))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break --) full-postfix-expression-decrement
        (constrain (constrain :postfix-expression-or-super))
        ((eval e)
         (const r reference ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch decrement-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return a))))
    
    (rule :full-new-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new
        ((constrain s)
         ((constrain :full-new-subexpression) s)
         ((constrain :arguments) s))
        ((eval e)
         (const f object (read-reference ((eval :full-new-subexpression) e)))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch construct-table null null f args))))
      (production :full-new-expression (new :full-super-expression :arguments) full-new-expression-super-new
        ((constrain s)
         ((constrain :full-super-expression) s)
         ((constrain :arguments) s))
        ((eval e)
         (const f object (read-reference ((eval :full-super-expression) e)))
         (const sf class ((super :full-super-expression) e))
         (const args argument-list ((eval :arguments) e))
         (return (unary-dispatch construct-table sf null f args)))))
    
    (rule :full-new-subexpression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression
        (constrain (constrain :primary-expression))
        (eval (eval :primary-expression)))
      (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier
        (constrain (constrain :qualified-identifier))
        (eval (eval :qualified-identifier)))
      (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression
        (constrain (constrain :full-new-expression))
        (eval (eval :full-new-expression)))
      (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator
        ((constrain s)
         ((constrain :full-new-subexpression) s)
         ((constrain :member-operator) s))
        ((eval e)
         (const a object (read-reference ((eval :full-new-subexpression) e)))
         (return ((eval :member-operator) e a))))
      (production :full-new-subexpression (:super-expression :dot-operator) full-new-subexpression-super-dot-operator
        ((constrain s)
         ((constrain :super-expression) s)
         ((constrain :dot-operator) s))
        ((eval e)
         (const a object (read-reference ((eval :super-expression) e)))
         (const sa class ((super :super-expression) e))
         (return ((eval :dot-operator) e a sa)))))
    
    (rule :short-new-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :short-new-expression (new :short-new-subexpression) short-new-expression-new
        (constrain (constrain :short-new-subexpression))
        ((eval e)
         (const f object (read-reference ((eval :short-new-subexpression) e)))
         (return (unary-dispatch construct-table null null f (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :short-new-expression (new :super-expression) short-new-expression-super-new
        (constrain (constrain :super-expression))
        ((eval e)
         (const f object (read-reference ((eval :super-expression) e)))
         (const sf class ((super :super-expression) e))
         (return (unary-dispatch construct-table sf null f (new argument-list (vector-of object) (list-set-of named-argument)))))))
    
    (rule :short-new-subexpression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full
        (constrain (constrain :full-new-subexpression))
        (eval (eval :full-new-subexpression)))
      (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short
        (constrain (constrain :short-new-expression))
        (eval (eval :short-new-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Member Operators")
    (rule :member-operator ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) reference)))
      (production :member-operator (:dot-operator) member-operator-dot-operator
        (constrain (constrain :dot-operator))
        ((eval e a) (return ((eval :dot-operator) e a null))))
      (production :member-operator (\. :paren-expression) member-operator-indirect
        (constrain (constrain :paren-expression))
        ((eval (e :unused) (a :unused)) (todo))))
    
    (rule :dot-operator ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object class-opt) reference)))
      (production :dot-operator (\. :qualified-identifier) dot-operator-qualified-identifier
        (constrain (constrain :qualified-identifier))
        ((eval e a sa)
         (const n partial-name ((name :qualified-identifier) e))
         (return (new dot-reference a sa n))))
      (production :dot-operator (:brackets) dot-operator-brackets
        (constrain (constrain :brackets))
        ((eval e a sa)
         (const args argument-list ((eval :brackets) e))
         (return (new bracket-reference a sa args)))))
    
    (rule :brackets ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) argument-list)))
      (production :brackets ([ ]) brackets-none
        ((constrain (s :unused)))
        ((eval (e :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed
        (constrain (constrain :list-expression))
        ((eval e)
         (const positional (vector object) ((eval-as-list :list-expression) e))
         (return (new argument-list positional (list-set-of named-argument)))))
      (production :brackets ([ :named-argument-list ]) brackets-named
        ((constrain s) (exec ((constrain :named-argument-list) s)))
        (eval (eval :named-argument-list))))
    
    (rule :arguments ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) argument-list)))
      (production :arguments (:paren-expressions) arguments-paren-expressions
        (constrain (constrain :paren-expressions))
        (eval (eval :paren-expressions)))
      (production :arguments (\( :named-argument-list \)) arguments-named
        ((constrain s) (exec ((constrain :named-argument-list) s)))
        (eval (eval :named-argument-list))))
    
    (rule :paren-expressions ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) argument-list)))
      (production :paren-expressions (\( \)) paren-expressions-none
        ((constrain (s :unused)))
        ((eval (e :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :paren-expressions (:paren-list-expression) paren-expressions-some
        (constrain (constrain :paren-list-expression))
        ((eval e)
         (const positional (vector object) ((eval-as-list :paren-list-expression) e))
         (return (new argument-list positional (list-set-of named-argument))))))
    
    (rule :named-argument-list ((constrain (-> (constraint-env) (list-set string))) (eval (-> (dynamic-env) argument-list)))
      (production :named-argument-list (:literal-field) named-argument-list-one
        (constrain (constrain :literal-field))
        ((eval e)
         (const na named-argument ((eval :literal-field) e))
         (return (new argument-list (vector-of object) (list-set na)))))
      (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed
        ((constrain s)
         ((constrain :list-expression) s)
         (return ((constrain :literal-field) s)))
        ((eval e)
         (const positional (vector object) ((eval-as-list :list-expression) e))
         (const na named-argument ((eval :literal-field) e))
         (return (new argument-list positional (list-set na)))))
      (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more
        ((constrain s)
         (const names1 (list-set string) ((constrain :named-argument-list) s))
         (const names2 (list-set string) ((constrain :literal-field) s))
         (rwhen (nonempty (set* names1 names2))
           (throw syntax-error))
         (return (set+ names1 names2)))
        ((eval e)
         (const args argument-list ((eval :named-argument-list) e))
         (const na named-argument ((eval :literal-field) e))
         (rwhen (some (& named args) na2 (= (& name na2) (& name na) string))
           (throw argument-mismatch-error))
         (return (new argument-list (& positional args) (set+ (& named args) (list-set na)))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Unary Operators")
    (rule :unary-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :unary-expression (:postfix-expression) unary-expression-postfix
        (constrain (constrain :postfix-expression))
        (eval (eval :postfix-expression)))
      (production :unary-expression (delete :postfix-expression) unary-expression-delete
        (constrain (constrain :postfix-expression))
        ((eval e) (return (delete-reference ((eval :postfix-expression) e)))))
      (production :unary-expression (void :unary-expression) unary-expression-void
        (constrain (constrain :unary-expression))
        ((eval e)
         (exec (read-reference ((eval :unary-expression) e)))
         (return undefined)))
      (production :unary-expression (typeof :unary-expression) unary-expression-typeof
        (constrain (constrain :unary-expression))
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
        (constrain (constrain :postfix-expression-or-super))
        ((eval e)
         (const r reference ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch increment-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return b)))
      (production :unary-expression (-- :postfix-expression-or-super) unary-expression-decrement
        (constrain (constrain :postfix-expression-or-super))
        ((eval e)
         (const r reference ((eval :postfix-expression-or-super) e))
         (const a object (read-reference r))
         (const sa class-opt ((super :postfix-expression-or-super) e))
         (const b object (unary-dispatch decrement-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return b)))
      (production :unary-expression (+ :unary-expression-or-super) unary-expression-plus
        (constrain (constrain :unary-expression-or-super))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch plus-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (- :unary-expression-or-super) unary-expression-minus
        (constrain (constrain :unary-expression-or-super))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch minus-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (~ :unary-expression-or-super) unary-expression-bitwise-not
        (constrain (constrain :unary-expression-or-super))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :unary-expression-or-super) e))
         (return (unary-dispatch bitwise-not-table sa null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (! :unary-expression) unary-expression-logical-not
        (constrain (constrain :unary-expression))
        ((eval e)
         (const a object (read-reference ((eval :unary-expression) e)))
         (return (unary-not a)))))
    
    (rule :unary-expression-or-super ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :unary-expression-or-super (:unary-expression) unary-expression-or-super-unary-expression
        (constrain (constrain :unary-expression))
        (eval (eval :unary-expression))
        ((super (e :unused)) (return null)))
      (production :unary-expression-or-super (:super-expression) unary-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Multiplicative Operators")
    (rule :multiplicative-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        (constrain (constrain :unary-expression))
        (eval (eval :unary-expression)))
      (production :multiplicative-expression (:multiplicative-expression-or-super * :unary-expression-or-super) multiplicative-expression-multiply
        ((constrain s)
         ((constrain :multiplicative-expression-or-super) s)
         ((constrain :unary-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch multiply-table sa sb a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super / :unary-expression-or-super) multiplicative-expression-divide
        ((constrain s)
         ((constrain :multiplicative-expression-or-super) s)
         ((constrain :unary-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch divide-table sa sb a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super % :unary-expression-or-super) multiplicative-expression-remainder
        ((constrain s)
         ((constrain :multiplicative-expression-or-super) s)
         ((constrain :unary-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const b object (read-reference ((eval :unary-expression-or-super) e)))
         (const sa class-opt ((super :multiplicative-expression-or-super) e))
         (const sb class-opt ((super :unary-expression-or-super) e))
         (return (binary-dispatch remainder-table sa sb a b)))))
    
    (rule :multiplicative-expression-or-super ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :multiplicative-expression-or-super (:multiplicative-expression) multiplicative-expression-or-super-multiplicative-expression
        (constrain (constrain :multiplicative-expression))
        (eval (eval :multiplicative-expression))
        ((super (e :unused)) (return null)))
      (production :multiplicative-expression-or-super (:super-expression) multiplicative-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Additive Operators")
    (rule :additive-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        (constrain (constrain :multiplicative-expression))
        (eval (eval :multiplicative-expression)))
      (production :additive-expression (:additive-expression-or-super + :multiplicative-expression-or-super) additive-expression-add
        ((constrain s)
         ((constrain :additive-expression-or-super) s)
         ((constrain :multiplicative-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :additive-expression-or-super) e)))
         (const b object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const sa class-opt ((super :additive-expression-or-super) e))
         (const sb class-opt ((super :multiplicative-expression-or-super) e))
         (return (binary-dispatch add-table sa sb a b))))
      (production :additive-expression (:additive-expression-or-super - :multiplicative-expression-or-super) additive-expression-subtract
        ((constrain s)
         ((constrain :additive-expression-or-super) s)
         ((constrain :multiplicative-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :additive-expression-or-super) e)))
         (const b object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const sa class-opt ((super :additive-expression-or-super) e))
         (const sb class-opt ((super :multiplicative-expression-or-super) e))
         (return (binary-dispatch subtract-table sa sb a b)))))
     
    (rule :additive-expression-or-super ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :additive-expression-or-super (:additive-expression) additive-expression-or-super-additive-expression
        (constrain (constrain :additive-expression))
        (eval (eval :additive-expression))
        ((super (e :unused)) (return null)))
      (production :additive-expression-or-super (:super-expression) additive-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Bitwise Shift Operators")
    (rule :shift-expression ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        (constrain (constrain :additive-expression))
        (eval (eval :additive-expression)))
      (production :shift-expression (:shift-expression-or-super << :additive-expression-or-super) shift-expression-left
        ((constrain s)
         ((constrain :shift-expression-or-super) s)
         ((constrain :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-left-table sa sb a b))))
      (production :shift-expression (:shift-expression-or-super >> :additive-expression-or-super) shift-expression-right-signed
        ((constrain s)
         ((constrain :shift-expression-or-super) s)
         ((constrain :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-right-table sa sb a b))))
      (production :shift-expression (:shift-expression-or-super >>> :additive-expression-or-super) shift-expression-right-unsigned
        ((constrain s)
         ((constrain :shift-expression-or-super) s)
         ((constrain :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return (binary-dispatch shift-right-unsigned-table sa sb a b)))))
    
    (rule :shift-expression-or-super ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :shift-expression-or-super (:shift-expression) shift-expression-or-super-shift-expression
        (constrain (constrain :shift-expression))
        (eval (eval :shift-expression))
        ((super (e :unused)) (return null)))
      (production :shift-expression-or-super (:super-expression) shift-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Relational Operators")
    (rule (:relational-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
        (constrain (constrain :shift-expression))
        (eval (eval :shift-expression)))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) < :shift-expression-or-super) relational-expression-less
        ((constrain s)
         ((constrain :relational-expression-or-super) s)
         ((constrain :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-table sa sb a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) > :shift-expression-or-super) relational-expression-greater
        ((constrain s)
         ((constrain :relational-expression-or-super) s)
         ((constrain :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-table sb sa b a))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) <= :shift-expression-or-super) relational-expression-less-or-equal
        ((constrain s)
         ((constrain :relational-expression-or-super) s)
         ((constrain :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-or-equal-table sa sb a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) >= :shift-expression-or-super) relational-expression-greater-or-equal
        ((constrain s)
         ((constrain :relational-expression-or-super) s)
         ((constrain :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return (binary-dispatch less-or-equal-table sb sa b a))))
      (production (:relational-expression :beta) ((:relational-expression :beta) is :shift-expression) relational-expression-is
        ((constrain s)
         ((constrain :relational-expression) s)
         ((constrain :shift-expression) s))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((constrain s)
         ((constrain :relational-expression) s)
         ((constrain :shift-expression) s))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression-or-super) relational-expression-in
        ((constrain s)
         ((constrain :relational-expression) s)
         ((constrain :shift-expression-or-super) s))
        ((eval (e :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((constrain s)
         ((constrain :relational-expression) s)
         ((constrain :shift-expression) s))
        ((eval (e :unused)) (todo))))
    
    (rule (:relational-expression-or-super :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:relational-expression-or-super :beta) ((:relational-expression :beta)) relational-expression-or-super-relational-expression
        (constrain (constrain :relational-expression))
        (eval (eval :relational-expression))
        ((super (e :unused)) (return null)))
      (production (:relational-expression-or-super :beta) (:super-expression) relational-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Equality Operators")
    (rule (:equality-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        (constrain (constrain :relational-expression))
        (eval (eval :relational-expression)))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) == (:relational-expression-or-super :beta)) equality-expression-equal
        ((constrain s)
         ((constrain :equality-expression-or-super) s)
         ((constrain :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (binary-dispatch equal-table sa sb a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) != (:relational-expression-or-super :beta)) equality-expression-not-equal
        ((constrain s)
         ((constrain :equality-expression-or-super) s)
         ((constrain :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not (binary-dispatch equal-table sa sb a b)))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) === (:relational-expression-or-super :beta)) equality-expression-strict-equal
        ((constrain s)
         ((constrain :equality-expression-or-super) s)
         ((constrain :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (binary-dispatch strict-equal-table sa sb a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) !== (:relational-expression-or-super :beta)) equality-expression-strict-not-equal
        ((constrain s)
         ((constrain :equality-expression-or-super) s)
         ((constrain :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not (binary-dispatch strict-equal-table sa sb a b))))))
    
    (rule (:equality-expression-or-super :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:equality-expression-or-super :beta) ((:equality-expression :beta)) equality-expression-or-super-equality-expression
        (constrain (constrain :equality-expression))
        (eval (eval :equality-expression))
        ((super (e :unused)) (return null)))
      (production (:equality-expression-or-super :beta) (:super-expression) equality-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        (constrain (constrain :equality-expression))
        (eval (eval :equality-expression)))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression-or-super :beta) & (:equality-expression-or-super :beta)) bitwise-and-expression-and
        ((constrain s)
         ((constrain :bitwise-and-expression-or-super) s)
         ((constrain :equality-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-and-expression-or-super) e)))
         (const b object (read-reference ((eval :equality-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-and-expression-or-super) e))
         (const sb class-opt ((super :equality-expression-or-super) e))
         (return (binary-dispatch bitwise-and-table sa sb a b)))))
    
    (rule (:bitwise-xor-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        (constrain (constrain :bitwise-and-expression))
        (eval (eval :bitwise-and-expression)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression-or-super :beta) ^ (:bitwise-and-expression-or-super :beta)) bitwise-xor-expression-xor
        ((constrain s)
         ((constrain :bitwise-xor-expression-or-super) s)
         ((constrain :bitwise-and-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-xor-expression-or-super) e)))
         (const b object (read-reference ((eval :bitwise-and-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-xor-expression-or-super) e))
         (const sb class-opt ((super :bitwise-and-expression-or-super) e))
         (return (binary-dispatch bitwise-xor-table sa sb a b)))))
    
    (rule (:bitwise-or-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        (constrain (constrain :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression)))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression-or-super :beta) \| (:bitwise-xor-expression-or-super :beta)) bitwise-or-expression-or
        ((constrain s)
         ((constrain :bitwise-or-expression-or-super) s)
         ((constrain :bitwise-xor-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :bitwise-or-expression-or-super) e)))
         (const b object (read-reference ((eval :bitwise-xor-expression-or-super) e)))
         (const sa class-opt ((super :bitwise-or-expression-or-super) e))
         (const sb class-opt ((super :bitwise-xor-expression-or-super) e))
         (return (binary-dispatch bitwise-or-table sa sb a b)))))
    
    
    (rule (:bitwise-and-expression-or-super :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-and-expression-or-super :beta) ((:bitwise-and-expression :beta)) bitwise-and-expression-or-super-bitwise-and-expression
        (constrain (constrain :bitwise-and-expression))
        (eval (eval :bitwise-and-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-and-expression-or-super :beta) (:super-expression) bitwise-and-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-xor-expression-or-super :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-xor-expression-or-super :beta) ((:bitwise-xor-expression :beta)) bitwise-xor-expression-or-super-bitwise-xor-expression
        (constrain (constrain :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-xor-expression-or-super :beta) (:super-expression) bitwise-xor-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-or-expression-or-super :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-or-expression-or-super :beta) ((:bitwise-or-expression :beta)) bitwise-or-expression-or-super-bitwise-or-expression
        (constrain (constrain :bitwise-or-expression))
        (eval (eval :bitwise-or-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-or-expression-or-super :beta) (:super-expression) bitwise-or-expression-or-super-super
        (constrain (constrain :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Binary Logical Operators")
    (rule (:logical-and-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        (constrain (constrain :bitwise-or-expression))
        (eval (eval :bitwise-or-expression)))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((constrain s)
         ((constrain :logical-and-expression) s)
         ((constrain :bitwise-or-expression) s))
        ((eval e)
         (const a object (read-reference ((eval :logical-and-expression) e)))
         (if (to-boolean a)
           (return (read-reference ((eval :bitwise-or-expression) e)))
           (return a)))))
    
    (rule (:logical-xor-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        (constrain (constrain :logical-and-expression))
        (eval (eval :logical-and-expression)))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((constrain s)
         ((constrain :logical-xor-expression) s)
         ((constrain :logical-and-expression) s))
        ((eval e)
         (const a object (read-reference ((eval :logical-xor-expression) e)))
         (const b object (read-reference ((eval :logical-and-expression) e)))
         (const ab boolean (to-boolean a))
         (const bb boolean (to-boolean b))
         (return (xor ab bb)))))
    
    (rule (:logical-or-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        (constrain (constrain :logical-xor-expression))
        (eval (eval :logical-xor-expression)))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((constrain s)
         ((constrain :logical-or-expression) s)
         ((constrain :logical-xor-expression) s))
        ((eval e)
         (const a object (read-reference ((eval :logical-or-expression) e)))
         (if (to-boolean a) 
           (return a)
           (return (read-reference ((eval :logical-xor-expression) e)))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Conditional Operator")
    (rule (:conditional-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        (constrain (constrain :logical-or-expression))
        (eval (eval :logical-or-expression)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((constrain s)
         ((constrain :logical-or-expression) s)
         ((constrain :assignment-expression 1) s)
         ((constrain :assignment-expression 2) s))
        ((eval e)
         (if (to-boolean (read-reference ((eval :logical-or-expression) e)))
           (return ((eval :assignment-expression 1) e))
           (return ((eval :assignment-expression 2) e))))))
    
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or)
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Assignment Operators")
    (rule (:assignment-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        (constrain (constrain :conditional-expression))
        (eval (eval :conditional-expression)))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((constrain s)
         ((constrain :postfix-expression) s)
         ((constrain :assignment-expression) s))
        ((eval e)
         (const r reference ((eval :postfix-expression) e))
         (const a object (read-reference ((eval :assignment-expression) e)))
         (write-reference r a)
         (return a)))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((constrain s)
         ((constrain :postfix-expression-or-super) s)
         ((constrain :assignment-expression) s))
        ((eval e)
         (return (eval-assignment-op (table :compound-assignment) ((super :postfix-expression-or-super) e) null
                                     (eval :postfix-expression-or-super) (eval :assignment-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment :super-expression) assignment-expression-compound-super
        ((constrain s)
         ((constrain :postfix-expression-or-super) s)
         ((constrain :super-expression) s))
        ((eval e)
         (return (eval-assignment-op (table :compound-assignment) ((super :postfix-expression-or-super) e) ((super :super-expression) e)
                                     (eval :postfix-expression-or-super) (eval :super-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((constrain s)
         ((constrain :postfix-expression) s)
         ((constrain :assignment-expression) s))
        ((eval (e :unused)) (todo))))
    
    (define (eval-assignment-op (table binary-table) (left-limit class-opt) (right-limit class-opt)
                                (left-eval (-> (dynamic-env) reference)) (right-eval (-> (dynamic-env) reference)) (e dynamic-env)) reference
      (const r-left reference (left-eval e))
      (const v-left object (read-reference r-left))
      (const v-right object (read-reference (right-eval e)))
      (const result object (binary-dispatch table left-limit right-limit v-left v-right))
      (write-reference r-left result)
      (return result))
    
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
    
    (production :logical-assignment (&&=) logical-assignment-logical-and)
    (production :logical-assignment (^^=) logical-assignment-logical-xor)
    (production :logical-assignment (\|\|=) logical-assignment-logical-or)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Comma Expressions")
    (rule (:list-expression :beta) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) reference)) (eval-as-list (-> (dynamic-env) (vector object))))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        (constrain (constrain :assignment-expression))
        (eval (eval :assignment-expression))
        ((eval-as-list e)
         (const elt object (read-reference ((eval :assignment-expression) e)))
         (return (vector elt))))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((constrain s)
         ((constrain :list-expression) s)
         ((constrain :assignment-expression) s))
        ((eval e)
         (exec (read-reference ((eval :list-expression) e)))
         (return ((eval :assignment-expression) e)))
        ((eval-as-list e)
         (const elts (vector object) ((eval-as-list :list-expression) e))
         (const elt object (read-reference ((eval :assignment-expression) e)))
         (return (append elts (vector elt))))))
    
    (production :optional-expression ((:list-expression allow-in)) optional-expression-expression)
    (production :optional-expression () optional-expression-empty)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Type Expressions")
    (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%section "Statements")
    
    (grammar-argument :omega
                      abbrev       ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)        ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (rule (:statement :omega) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:statement :omega) (:empty-statement) statement-empty-statement
        ((constrain (s :unused)))
        ((eval (e :unused) d) (return d)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        (constrain (constrain :expression-statement))
        ((eval e (d :unused)) (return ((eval :expression-statement) e))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:annotated-block) statement-annotated-block
        (constrain (constrain :annotated-block))
        (eval (eval :annotated-block)))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        (constrain (constrain :labeled-statement))
        (eval (eval :labeled-statement)))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        (constrain (constrain :if-statement))
        (eval (eval :if-statement)))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        (constrain (constrain :continue-statement))
        (eval (eval :continue-statement)))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        (constrain (constrain :break-statement))
        (eval (eval :break-statement)))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        (constrain (constrain :return-statement))
        ((eval e (d :unused)) (return ((eval :return-statement) e))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    
    (rule (:substatement :omega) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        (constrain (constrain :statement))
        (eval (eval :statement)))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo))))
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon no-short-if) () semicolon-no-short-if)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%subsection "Expression Statement")
    (rule :expression-statement ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        (constrain (constrain :list-expression))
        ((eval e) (return (read-reference ((eval :list-expression) e))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Super Statement")
    (production :super-statement (super :arguments) super-statement-super-arguments)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Block Statement")
    (rule :annotated-block ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production :annotated-block (:attributes :block) annotated-block-attributes-and-block
        (constrain (constrain :block)) ;******
        (eval (eval :block)))) ;******
    
    (rule :block ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production :block ({ :directives }) block-directives
        (constrain (constrain :directives))
        (eval (eval :directives))))
    
    (rule :directives ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production :directives () directives-none
        ((constrain (s :unused)))
        ((eval (e :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((constrain s)
         ((constrain :directives-prefix) s)
         ((constrain :directive) s))
        ((eval e d)
         (const v object ((eval :directive) e d))
         (return ((eval :directives-prefix) e v)))))
    
    (rule :directives-prefix ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production :directives-prefix () directives-prefix-none
        ((constrain (s :unused)))
        ((eval (e :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((constrain s)
         ((constrain :directives-prefix) s)
         ((constrain :directive) s))
        ((eval e d)
         (const v object ((eval :directive) e d))
         (return ((eval :directives-prefix) e v)))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Labeled Statements")
    (rule (:labeled-statement :omega) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((constrain s) ((constrain :substatement) (add-label s (name :identifier))))
        ((eval e d)
         (catch ((return ((eval :substatement) e d)))
           (x) (if (and (in x go-break :narrow-true) (= (& label x) (name :identifier) string))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "If Statement")
    (rule (:if-statement :omega) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:if-statement abbrev) (if :paren-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((constrain s)
         ((constrain :paren-list-expression) s)
         ((constrain :substatement) s))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) e)))
           (return ((eval :substatement) e d))
           (return d))))
      (production (:if-statement full) (if :paren-list-expression (:substatement full)) if-statement-if-then-full
        ((constrain s)
         ((constrain :paren-list-expression) s)
         ((constrain :substatement) s))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) e)))
           (return ((eval :substatement) e d))
           (return d))))
      (production (:if-statement :omega) (if :paren-list-expression (:substatement no-short-if) else (:substatement :omega))
                  if-statement-if-then-else
        ((constrain s)
         ((constrain :paren-list-expression) s)
         ((constrain :substatement 1) s)
         ((constrain :substatement 2) s))
        ((eval e d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) e)))
           (return ((eval :substatement 1) e d))
           (return ((eval :substatement 2) e d))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Switch Statement")
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
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Do-While Statement")
    (production :do-statement (do (:substatement abbrev) while :paren-list-expression) do-statement-do-while)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "While Statement")
    (production (:while-statement :omega) (while :paren-list-expression (:substatement :omega)) while-statement-while)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "For Statements")
    (production (:for-statement :omega) (for \( :for-initialiser \; :optional-expression \; :optional-expression \)
                                             (:substatement :omega)) for-statement-c-style)
    (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:substatement :omega)) for-statement-in)
    
    (production :for-initialiser () for-initialiser-empty)
    (production :for-initialiser ((:list-expression no-in)) for-initialiser-expression)
    (production :for-initialiser (:attributes :variable-definition-kind (:variable-binding-list no-in)) for-initialiser-variable-definition)
    
    (production :for-in-binding (:postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:attributes :variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "With Statement")
    (production (:with-statement :omega) (with :paren-list-expression (:substatement :omega)) with-statement-with)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Continue and Break Statements")
    (rule :continue-statement ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-continue d ""))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-continue d (name :identifier))))))
    
    (rule :break-statement ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-break d ""))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) d) (throw (new go-break d (name :identifier))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Return Statement")
    (rule :return-statement ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env) object)))
      (production :return-statement (return) return-statement-default
        ((constrain s)
         (when (not (& can-return s))
           (throw syntax-error)))
        ((eval (e :unused)) (throw (new go-return undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((constrain s)
         (when (not (& can-return s))
           (throw syntax-error))
         ((constrain :list-expression) s))
        ((eval e) (throw (new go-return (read-reference ((eval :list-expression) e)))))))
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Throw Statement")
    (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%subsection "Try Statement")
    (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses)
    (production :try-statement (try :block :finally-clause) try-statement-finally-clause)
    (production :try-statement (try :block :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
    
    (production :catch-clauses (:catch-clause) catch-clauses-one)
    (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
    
    (production :catch-clause (catch \( :parameter \) :block) catch-clause-block)
    
    (production :finally-clause (finally :block) finally-clause-block)
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    (%section "Directives")
    (rule (:directive :omega_2) ((constrain (-> (constraint-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        (constrain (constrain :statement))
        (eval (eval :statement)))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:directive :omega_2) (:attribute :no-line-break :attributes (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (production (:directive :omega_2) (:package-definition) directive-package-definition
        ((constrain (s :unused)) (todo))
        ((eval (e :unused) (d :unused)) (todo)))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((constrain (s :unused)) (todo))
          ((eval (e :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((constrain (s :unused)) (todo))
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
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
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
    (%print-actions ("Static Constraints" constrain) ("Evaluation" eval))
    
    
    
    (%subsection "Use Directive")
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
    
    
    (%subsection "Import Directive")
    (production :import-directive (import :import-binding :includes-excludes) import-directive-import)
    (production :import-directive (import :import-binding \, namespace :paren-list-expression :includes-excludes)
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
    (production :pragma-expr (:identifier :paren-list-expression) pragma-expr-identifier-and-arguments)
    
    
    (%section "Definitions")
    (%subsection "Export Definition")
    (production :export-definition (export :export-binding-list) export-definition-definition)
    
    (production :export-binding-list (:export-binding) export-binding-list-one)
    (production :export-binding-list (:export-binding-list \, :export-binding) export-binding-list-more)
    
    (production :export-binding (:function-name) export-binding-simple)
    (production :export-binding (:function-name = :function-name) export-binding-initialiser)
    
    
    (%subsection "Variable Definition")
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
          ((constrain :directives) initial-constraint-env)
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
