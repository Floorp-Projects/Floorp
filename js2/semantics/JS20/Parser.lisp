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
    
    (deftuple break (value object) (label label))
    (deftuple continue (value object) (label label))
    (deftuple returned-value (value object))
    (deftuple thrown-value (value object))
    (deftype early-exit (union break continue returned-value thrown-value))
    
    (deftype semantic-exception (union early-exit semantic-error))
    
    
    (%heading (2 :semantics) "Objects")
    (deftype object (union undefined null boolean float64 string namespace compound-attribute class method-closure prototype instance))
    
    
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
    
    (deftuple compound-attribute
      (namespaces (list-set namespace))
      (extend class-opt)
      (enumerable boolean)
      (dynamic boolean)
      (compile boolean)
      (member-mod member-modifier)
      (override-mod override-modifier)
      (prototype boolean)
      (unused boolean))
    
    (deftype attribute (union boolean namespace compound-attribute))
    (deftype attribute-not-false (union (tag true) namespace compound-attribute))
    
    
    (%heading (3 :semantics) "Classes")
    (defrecord class
      (super class-opt)
      (prototype object)
      (static-members (list-set static-member) :var)
      (instance-members (list-set instance-member) :var)
      (dynamic boolean)
      (primitive boolean)
      (private-namespace namespace)
      (call invoker)
      (construct invoker))
    (deftype class-opt (union null class))
    
    (define (make-built-in-class (superclass class-opt) (dynamic boolean) (primitive boolean)) class
      (const private-namespace namespace (new namespace "private"))
      (function (call (this object :unused) (args argument-list :unused)) object
        (todo))
      (function (construct (this object :unused) (args argument-list :unused)) object
        (todo))
      (return (new class superclass null (list-set-of static-member) (list-set-of instance-member)
                   dynamic primitive private-namespace call construct)))
    
    (define object-class class (make-built-in-class null false true))
    (define undefined-class class (make-built-in-class object-class false true))
    (define null-class class (make-built-in-class object-class false true))
    (define boolean-class class (make-built-in-class object-class false true))
    (define number-class class (make-built-in-class object-class false true))
    (define string-class class (make-built-in-class object-class false false))
    (define character-class class (make-built-in-class string-class false false))
    (define namespace-class class (make-built-in-class object-class false false))
    (define attribute-class class (make-built-in-class object-class false false))
    (define class-class class (make-built-in-class object-class false false))
    (define function-class class (make-built-in-class object-class false false))
    (define prototype-class class (make-built-in-class object-class true false))
    
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
    (deftag read)
    (deftag write)
    (deftag read-write)
    (deftype access (tag read write read-write))
    (deftype instance-data (union slot-id method accessor))
    
    (defrecord instance-member 
      (partial-name partial-name)
      (access access)
      (category (tag abstract virtual final))
      (indexable boolean)
      (enumerable boolean)
      (data (union instance-data namespace)))
    
    (deftype static-data (union variable method accessor))
    
    (defrecord static-member
      (partial-name partial-name)
      (access access)
      (category (tag static constructor))
      (indexable boolean)
      (enumerable boolean)
      (data (union static-data namespace)))
    (deftype member (union instance-member static-member))
    (deftype member-data (union instance-data static-data))
    (deftype member-data-opt (union null member-data))
    
    (defrecord variable
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
    
    (defrecord dynamic-property 
      (name string)
      (value object :var))
    
    
    (%heading (3 :semantics) "Class Instances")
    (deftype instance (union fixed-instance dynamic-instance))
    (deftype instance-opt (union null instance))
    
    (defrecord fixed-instance
      (type class)
      (call invoker)
      (construct invoker)
      (typeof-string string)
      (slots (list-set slot) :var))
    
    (defrecord dynamic-instance
      (type class)
      (call invoker)
      (construct invoker)
      (typeof-string string)
      (slots (list-set slot) :var)
      (dynamic-properties (list-set dynamic-property) :var))
    
    (%heading (4 :semantics) "Slots")
    (defrecord slot
      (id slot-id)
      (value object :var))
    
    (defrecord slot-id (type class))
    
    
    (%heading (2 :semantics) "Qualified Names")
    (deftuple qualified-name (namespace namespace) (name string))
    
    (deftuple partial-name (namespaces (list-set namespace)) (name string))
    
    
    (%heading (2 :semantics) "Objects with Limits")
    (%text :comment (:label limited-instance instance) " must be an instance of " (:label limited-instance limit) " or one of "
           (:label limited-instance limit) :apostrophe "s descendants.")
    (deftuple limited-instance
      (instance instance)
      (limit class))
    
    (deftype obj-optional-limit (union object limited-instance))
    
    
    (%heading (2 :semantics) "References")
    (deftuple variable-reference
      (env frame)
      (partial-name partial-name))
    
    (deftuple dot-reference
      (base obj-optional-limit)
      (prop-name partial-name))
    
    (deftuple bracket-reference
      (base obj-optional-limit)
      (args argument-list))
    
    (deftype reference (union variable-reference dot-reference bracket-reference))
    (deftype obj-or-ref (union object reference))
    
    
    (deftuple limited-obj-or-ref
      (ref obj-or-ref)
      (limit class))
    
    (deftype obj-or-ref-optional-limit (union obj-or-ref limited-obj-or-ref))
    
    
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
    
    
    (%heading (2 :semantics) "Contexts")
    (deftuple context
      (strict boolean)
      (unchecked boolean)
      (inside-function boolean)
      (private-namespace namespace-opt)
      (open-namespaces (list-set namespace))
      (break-labels (list-set label))
      (continue-labels (list-set label)))
    
    (define initial-context context
      (new context false false false null (list-set public-namespace) (list-set-of label) (list-set-of label)))
    
    
    (%heading (3 :semantics) "Labels")
    (deftag default)
    (deftype label (union string (tag default)))
    
    
    (%heading (2 :semantics) "Environments")
    (deftype frame (union compile-frame runtime-frame))
    
    
    (%heading (3 :semantics) "Compile Environments")
    (deftype compile-frame (union system-compile-frame package-compile-frame block-compile-frame function-compile-frame class-compile-frame substatement-frame))
    (deftype compile-frame-opt (union compile-frame null))

    (defrecord system-compile-frame
      (parent null)
      (bindings (vector compile-or-anti-binding) :var))

    (defrecord package-compile-frame
      (parent compile-frame)
      (bindings (vector compile-or-anti-binding) :var)
      (internal-namespace namespace))

    (defrecord block-compile-frame
      (parent compile-frame)
      (bindings (vector compile-or-anti-binding) :var))

    (defrecord function-compile-frame
      (parent compile-frame)
      (bindings (vector compile-or-anti-binding) :var)
      (has-this boolean))

    (defrecord class-compile-frame
      (parent compile-frame)
      (bindings (vector compile-or-anti-binding) :var)
      (private-namespace namespace))

    (defrecord substatement-frame
      (parent compile-frame))
    
    (define initial-compile-frame system-compile-frame (new system-compile-frame null (vector-of compile-or-anti-binding)))
    
    
    (%heading (3 :semantics) "Dynamic Environments")
    (defrecord runtime-frame
      (parent runtime-frame-opt)
      (bindings (vector runtime-or-anti-binding) :var))
    
    (deftype runtime-frame-opt (union runtime-frame null))
    
    #|(deftuple definition 
      (name qualified-name)
      (type class)
      (data (union slot object accessor)))|#
    
    (define initial-runtime-frame runtime-frame (new runtime-frame null (vector-of runtime-or-anti-binding)))
    
    
    (%heading (3 :semantics) "Environment Bindings")
    (deftype compile-or-anti-binding (union compile-binding antibinding))

    (deftag unknown)
    (defrecord compile-binding
      (partial-name partial-name)
      (access access)
      (compile-value (union object (tag unknown)) :var)
      (type (union class (tag unknown)))
      (old-style-var boolean))
    
    (defrecord antibinding
      (partial-name partial-name)
      (access access))
    
    
    (deftype runtime-or-anti-binding (union runtime-binding antibinding))
    (defrecord runtime-binding
      (partial-name partial-name)
      (access access)
      (data static-data))
    
    
    
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
          (if (= (length o) 1)
            (return character-class)
            (return string-class)))
        (:select namespace (return namespace-class))
        (:select compound-attribute (return attribute-class))
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
        (:select (union namespace compound-attribute class method-closure prototype) (return true))
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
        (:select (union namespace compound-attribute class method-closure) (throw type-error))
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
        (:select compound-attribute (todo))
        (:select class (todo))
        (:select method-closure (todo))
        (:select (union prototype instance) (todo))))
    
    (%heading (3 :semantics) (:global to-primitive nil))
    (define (to-primitive (o object) (hint object :unused)) object
      (case o
        (:select (union undefined null boolean float64 string) (return o))
        (:select (union namespace compound-attribute class method-closure prototype instance) (return (to-string o)))))
    
    
    (%heading (3 :semantics) (:global unary-plus nil))
    (%text :comment (:global-call unary-plus o) " returns the value of the unary expression " (:character-literal "+") (:local o) ".")
    (define (unary-plus (a obj-optional-limit)) object
      (return (unary-dispatch plus-table null a (new argument-list (vector-of object) (list-set-of named-argument)))))
    
    (%heading (3 :semantics) (:global unary-not nil))
    (%text :comment (:global-call unary-not o) " returns the value of the unary expression " (:character-literal "!") (:local o) ".")
    (define (unary-not (a object)) object
      (return (not (to-boolean a))))
    
    
    (%heading (3 :semantics) "Attributes")
    (%text :comment (:global-call combine-attributes a b) " returns the attribute that results from concatenating the attributes "
           (:local a) " and " (:local b) ".")
    (define (combine-attributes (a attribute-not-false) (b attribute)) attribute
      (cond
       ((in b false-type :narrow-false) (return false))
       ((in a true-type :narrow-false) (return b))
       ((in b true-type :narrow-false) (return a))
       ((in a namespace :narrow-both)
        (cond
         ((= a b attribute) (return a))
         ((in b namespace :narrow-both)
          (return (new compound-attribute (list-set a b) null false false false null null false false)))
         (nil
          (return (new compound-attribute
                       (set+ (& namespaces b) (list-set a))
                       (& extend b) (& enumerable b) (& dynamic b) (& compile b) (& member-mod b) (& override-mod b) (& prototype b) (& unused b))))))
       ((in b namespace :narrow-both)
        (return (new compound-attribute
                     (set+ (& namespaces a) (list-set b))
                     (& extend a) (& enumerable a) (& dynamic a) (& compile a) (& member-mod a) (& override-mod a) (& prototype a) (& unused a))))
       (nil
        (// "Both " (:local a) " and " (:local b)
            " are compound attributes. Ensure that they have no duplicate or conflicting contents other than namespaces.")
        (if (or (and (not-in (& extend a) null) (not-in (& extend b) null))
                (and (& enumerable a) (& enumerable b))
                (and (& dynamic a) (& dynamic b))
                (and (& compile a) (& compile b))
                (and (not-in (& member-mod a) null) (not-in (& member-mod b) null))
                (and (not-in (& override-mod a) null) (not-in (& override-mod b) null))
                (and (& prototype a) (& prototype b))
                (and (& unused a) (& unused b)))
          (throw type-error)
          (return (new compound-attribute
                       (set+ (& namespaces a) (& namespaces b))
                       (if (not-in (& extend a) null) (& extend a) (& extend b))
                       (or (& enumerable a) (& enumerable b))
                       (or (& dynamic a) (& dynamic b))
                       (or (& compile a) (& compile b))
                       (if (not-in (& member-mod a) null) (& member-mod a) (& member-mod b))
                       (if (not-in (& override-mod a) null) (& override-mod a) (& override-mod b))
                       (or (& prototype a) (& prototype b))
                       (or (& unused a) (& unused b))))))))
    
    
    (%heading (2 :semantics) "Objects with Limits")
    (%text :comment (:global-call get-object o) " returns " (:local o) " without its limit, if any.")
    (define (get-object (o obj-optional-limit)) object
      (case o
        (:narrow object (return o))
        (:narrow limited-instance (return (& instance o)))))
    
    (%text :comment (:global-call get-object-limit o) " returns " (:local o) :apostrophe "s limit or " (:tag null) " if none is provided.")
    (define (get-object-limit (o obj-optional-limit)) class-opt
      (case o
        (:narrow object (return null))
        (:narrow limited-instance (return (& limit o)))))
    
    
    (%heading (2 :semantics) "References")
    (%text :comment "If " (:local r) " is an " (:type object) ", " (:global-call read-reference r) " returns it unchanged.  If "
           (:local r) " is a " (:type reference) ", this function reads " (:local r) " and returns the result.")
    (define (read-reference (r obj-or-ref)) object
      (case r
        (:narrow object (return r))
        (:narrow variable-reference (return (read-variable (& env r) (& partial-name r))))
        (:narrow dot-reference (return (read-property (& base r) (& prop-name r))))
        (:narrow bracket-reference (return (unary-dispatch bracket-read-table null (& base r) (& args r))))))
    
    
    (%text :comment (:global-call read-ref-with-limit r) " reads the reference, if any, inside " (:local r)
           " and returns the result, retaining the same limit as " (:local r) ". If " (:local r)
           " has a limit " (:local limit) ", then the object read from the reference is checked to make sure that it is an instance of " (:local limit)
           " or one of its descendants.")
    (define (read-ref-with-limit (r obj-or-ref-optional-limit)) obj-optional-limit
      (case r
        (:narrow obj-or-ref (return (read-reference r)))
        (:narrow limited-obj-or-ref
          (const o object (read-reference (& ref r)))
          (const limit class (& limit r))
          (rwhen (= o null object)
            (return null))
          (rwhen (or (not-in o instance :narrow-false) (not (has-type o limit)))
            (throw type-error))
          (return (new limited-instance o limit)))))
    
    
    (%text :comment "If " (:local r) " is a reference, " (:global-call write-reference r o) " writes " (:local o) 
           " into " (:local r) ". An error occurs if " (:local r) " is not a reference. "
           (:local r) :apostrophe "s limit, if any, is ignored.")
    (define (write-reference (r obj-or-ref-optional-limit) (o object)) void
      (case r
        (:select object (throw reference-error))
        (:narrow variable-reference (write-variable (& env r) (& partial-name r) o))
        (:narrow dot-reference (write-property (& base r) (& prop-name r) o))
        (:narrow bracket-reference
          (const args argument-list (new argument-list (cons o (& positional (& args r))) (& named (& args r))))
          (exec (unary-dispatch bracket-write-table null (& base r) args)))
        (:narrow limited-obj-or-ref (write-reference (& ref r) o))))
    
    
    (%text :comment "If " (:local r) " is a " (:type reference) ", " (:global-call delete-reference r) " deletes it. If "
           (:local r) " is an " (:type object) ", this function signals an error.")
    (define (delete-reference (r obj-or-ref)) object
      (case r
        (:select object (throw reference-error))
        (:narrow variable-reference (return (delete-variable (& env r) (& partial-name r))))
        (:narrow dot-reference (return (delete-property (& base r) (& prop-name r))))
        (:narrow bracket-reference (return (unary-dispatch bracket-delete-table null (& base r) (& args r))))))
    
    
    (%text :comment (:global-call reference-base r) " returns " (:type reference) " " (:local r) :apostrophe "s base or"
           (:tag null) " if there is none. " (:local r) :apostrophe "s limit and the base" :apostrophe "s limit, if any, are ignored.")
    (define (reference-base (r obj-or-ref-optional-limit)) object
      (case r
        (:select (union object variable-reference) (return null))
        (:narrow (union dot-reference bracket-reference) (return (get-object (& base r))))
        (:narrow limited-obj-or-ref (return (reference-base (& ref r))))))
    
    
    (%heading (2 :semantics) "Slots")
    
    (define (find-slot (o object) (id slot-id)) slot
      (assert (in o instance :narrow-true) (:local o) " must be an " (:type instance) ";")
      (const matching-slots (list-set slot)
        (map (& slots o) s s (= (& id s) id slot-id)))
      (assert (= (length matching-slots) 1) "Note that exactly one slot should match: " (:assertion) ";")
      (return (elt-of matching-slots)))
    
    
    (%heading (2 :semantics) "Member Lookup")
    (%heading (3 :semantics) "Reading a Property")
    
    (define (read-property (ol obj-optional-limit) (pn partial-name)) object
      (const ns namespace (resolve-object-namespace (get-object ol) pn (list-set-of access read read-write)))
      (const qn qualified-name (new qualified-name ns (& name pn)))
      (return (read-qualified-property ol qn false)))
    
    
    (define (read-qualified-property (ol obj-optional-limit) (qn qualified-name) (indexable-only boolean)) object
      (var d member-data-opt)
      (reserve p)
      (case ol
        (:narrow (union undefined null boolean float64 string namespace compound-attribute method-closure fixed-instance)
          (<- d (most-specific-member (object-type ol) false qn (list-set-of access read read-write) indexable-only)))
        (:narrow class
          (<- d (most-specific-member ol true qn (list-set-of access read read-write) indexable-only)))
        (:narrow prototype
          (cond
           ((/= (& namespace qn) public-namespace namespace)
            (throw property-not-found-error))
           ((some (& dynamic-properties ol) p (= (& name p) (& name qn) string) :define-true)
            (return (& value p)))
           ((in (& parent ol) (tag null))
            (return undefined))
           (nil (return (read-qualified-property (& parent ol) qn indexable-only)))))
        (:narrow dynamic-instance
          (<- d (most-specific-member (object-type ol) false qn (list-set-of access read read-write) indexable-only))
          (rwhen (and (= d null member-data-opt) (= (& namespace qn) public-namespace namespace))
            (if (some (& dynamic-properties ol) p (= (& name p) (& name qn) string) :define-true)
              (return (& value p))
              (return undefined))))
        (:narrow limited-instance
          (<- d (most-specific-member (& super (& limit ol)) false qn (list-set-of access read read-write) indexable-only))))
      (const o object (get-object ol))
      (case d
        (:select (tag null) (throw property-not-found-error))
        (:narrow variable (return (& value d)))
        (:narrow slot-id (return (& value (find-slot o d))))
        (:narrow method (return (new method-closure o d)))
        (:narrow accessor (return ((& call (& f d)) o (new argument-list (vector-of object) (list-set-of named-argument)))))))
    
    
    (%heading (3 :semantics) "Writing a Property")
    
    (define (write-property (ol obj-optional-limit) (pn partial-name) (new-value object)) void
      (const ns namespace (resolve-object-namespace (get-object ol) pn (list-set-of access write read-write)))
      (const qn qualified-name (new qualified-name ns (& name pn)))
      (write-qualified-property ol qn false new-value))
    
    
    (define (write-qualified-property (ol obj-optional-limit) (qn qualified-name) (indexable-only boolean) (new-value object)) void
      (var d member-data-opt)
      (case ol
        (:select (union undefined null boolean float64 string namespace compound-attribute method-closure)
          (throw property-not-found-error))
        (:narrow class
          (<- d (most-specific-member ol true qn (list-set-of access write read-write) indexable-only)))
        (:narrow prototype
          (rwhen (/= (& namespace qn) public-namespace namespace)
            (throw property-not-found-error))
          (write-dynamic-property ol (& name qn) new-value)
          (return))
        (:narrow fixed-instance
          (<- d (most-specific-member (object-type ol) false qn (list-set-of access write read-write) indexable-only)))
        (:narrow dynamic-instance
          (<- d (most-specific-member (object-type ol) false qn (list-set-of access write read-write) indexable-only))
          (rwhen (and (= d null member-data-opt) (= (& namespace qn) public-namespace namespace))
            (<- d (most-specific-member (object-type ol) false qn (list-set-of access read write read-write) indexable-only))
            (rwhen (/= d null member-data-opt)
              (throw property-not-found-error))
            (write-dynamic-property ol (& name qn) new-value)
            (return)))
        (:narrow limited-instance
          (<- d (most-specific-member (& super (& limit ol)) false qn (list-set-of access write read-write) indexable-only))))
      (const o object (get-object ol))
      (assert (not-in d method :narrow-true)
              (:local d) " cannot be a " (:type method) " at this point because all " (:type method)
              " properties are read-only;")
      (case d
        (:select (tag null) (throw property-not-found-error))
        (:narrow variable
          (rwhen (not (relaxed-has-type new-value (& type d)))
            (throw type-error))
          (&= value d new-value))
        (:narrow slot-id
          (rwhen (not (relaxed-has-type new-value (& type d)))
            (throw type-error))
          (&= value (find-slot o d) new-value))
        (:narrow accessor
          (rwhen (not (relaxed-has-type new-value (& type d)))
            (throw type-error))
          (exec ((& call (& f d)) o (new argument-list (vector new-value) (list-set-of named-argument)))))))
    
    
    (define (write-dynamic-property (o (union prototype dynamic-instance)) (name string) (new-value object)) void
      (reserve p)
      (if (some (& dynamic-properties o) p (= (& name p) name string) :define-true)
        (&= value p new-value)
        (&= dynamic-properties o (set+ (& dynamic-properties o) (list-set (new dynamic-property name new-value))))))
    
    
    (define (delete-property (o obj-optional-limit :unused) (pn partial-name :unused)) boolean
      (todo))
    
    (define (delete-qualified-property (o object :unused) (name string :unused) (ns namespace :unused) (indexable-only boolean :unused)) boolean
      (todo))
    
    
    (define (most-specific-member (c class-opt) (static boolean) (qn qualified-name) (accesses (list-set access))
                                  (indexable-only boolean)) member-data-opt
      (rwhen (in c (tag null) :narrow-false)
        (return null))
      (var qn2 qualified-name qn)
      (const members (list-set member) (if static (& static-members c) (& instance-members c)))
      (reserve m)
      (when (some members m (and (set-in (& access m) accesses)
                                 (= (& name qn) (& name (& partial-name m)) string)
                                 (set-in (& namespace qn) (& namespaces (& partial-name m)))
                                 (or (not indexable-only) (& indexable m))) :define-true)
        (const d (union member-data namespace) (& data m))
        (rwhen (not-in d namespace :narrow-both)
          (return d))
        (<- qn2 (new qualified-name d (& name qn))))
      (return (most-specific-member (& super c) static qn2 accesses indexable-only)))
    
    
    (define (resolve-member-namespace (c class) (static boolean) (pn partial-name) (accesses (list-set access))) namespace-opt
      (const s class-opt (& super c))
      (when (not-in s (tag null) :narrow-true)
        (const ns namespace-opt (resolve-member-namespace s static pn accesses))
        (rwhen (not-in ns (tag null) :narrow-true)
          (return ns)))
      (const members (list-set member) (if static (& static-members c) (& instance-members c)))
      (const matches (list-set member) (map members m m (and (set-in (& access m) accesses)
                                                             (= (& name pn) (& name (& partial-name m)) string)
                                                             (nonempty (set* (& namespaces pn) (& namespaces (& partial-name m)))))))
      (rwhen (nonempty matches)
        (rwhen (> (length matches) 1)
          (// "This access is ambiguous because it found several different members in the same class.")
          (throw property-not-found-error))
        (/* "Let " (:local match) ":" :nbsp (:type member) " be the one element of " (:local matches) ".")
        (const match member (elt-of matches))
        (*/)
        (const matching-namespaces (list-set namespace) (set* (& namespaces pn) (& namespaces (& partial-name match))))
        (/* "Let " (:local ns2) ":" :nbsp (:type namespace) " be any element of " (:local matching-namespaces) ".")
        (const ns2 namespace (elt-of matching-namespaces))
        (*/)
        (return ns2))
      (return null))
    
    
    (define (resolve-object-namespace (o object) (pn partial-name) (accesses (list-set access))) namespace
      (const ns namespace-opt (if (in o class :narrow-true)
                                (resolve-member-namespace o true pn accesses)
                                (resolve-member-namespace (object-type o) false pn accesses)))
      (rwhen (not-in ns (tag null) :narrow-true)
        (return ns))
      (rwhen (set-in public-namespace (& namespaces pn))
        (return public-namespace))
      (throw property-not-found-error))
    
    
    
    (%heading (2 :semantics) "Operator Dispatch")
    (%heading (3 :semantics) "Unary Operators")
    (%text :comment (:global-call unary-dispatch table this operand args) " dispatches the unary operator described by " (:local table)
           " applied to the " (:character-literal "this") " value " (:local this) ", the operand " (:local operand)
           ", and zero or more positional and/or named arguments " (:local args)
           ". If " (:local operand) " has a non-" (:tag null)
           " limit class, lookup is restricted to operators defined on the proper ancestors of that limit.")
    (define (unary-dispatch (table (list-set unary-method)) (this object) (operand obj-optional-limit) (args argument-list)) object
      (const applicable-ops (list-set unary-method)
        (map table m m (limited-has-type operand (& operand-type m))))
      (reserve best)
      (rwhen (some applicable-ops best
                   (every applicable-ops m2 (is-ancestor (& operand-type m2) (& operand-type best))) :define-true)
        (return ((& f best) this (get-object operand) args)))
      (throw property-not-found-error))
    
    
    (%text :comment (:global-call limited-has-type o c) " returns " (:tag true) " if " (:local o) " is a member of class " (:local c)
           " with the added condition that, if " (:local o) " has a non-" (:tag null) " limit class " (:local limit) ", "
           (:local c) " is a proper ancestor of " (:local limit) ".")
    (define (limited-has-type (o obj-optional-limit) (c class)) boolean
      (const a object (get-object o))
      (const limit class-opt (get-object-limit o))
      (if (has-type a c)
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
    
    (%text :comment (:global-call binary-dispatch table left right) " dispatches the binary operator described by " (:local table)
           " applied to the operands " (:local left) " and " (:local right) ". If " (:local left) " has a non-" (:tag null)
           " limit " (:local left-limit) ", the lookup is restricted to operator definitions with an ancestor of " (:local left-limit)
           " for the left operand. Similarly, if " (:local right) " has a non-" (:tag null) " limit " (:local right-limit)
           ", the lookup is restricted to operator definitions with an ancestor of " (:local right-limit) " for the right operand.")
    (define (binary-dispatch (table (list-set binary-method)) (left obj-optional-limit) (right obj-optional-limit)) object
      (const applicable-ops (list-set binary-method)
        (map table m m (and (limited-has-type left (& left-type m))
                            (limited-has-type right (& right-type m)))))
      (reserve best)
      (rwhen (some applicable-ops best
                   (every applicable-ops m2 (is-binary-descendant best m2)) :define-true)
        (return ((& f best) (get-object left) (get-object right))))
      (throw property-not-found-error))
    
    
    (%heading (2 :semantics) "Contexts")
    
    (%text :comment (:global-call add-break-label c l) " returns a new " (:type context) " that is the same as "
           (:local c) " except that it includes the label " (:local l)
           " in the context" :apostrophe "s set of labels that are valid targets for a " (:character-literal "break") " statement.")
    (define (add-break-label (c context) (l label)) context
      (return (new context (& strict c) (& unchecked c) (& inside-function c) (& private-namespace c) (& open-namespaces c)
                   (set+ (& break-labels c) (list-set-of label l)) (& continue-labels c))))
    
    
    (%text :comment (:global-call add-continue-labels c ls) " returns a new " (:type context) " that is the same as "
           (:local c) " except that it includes the labels " (:local ls)
           " in the context" :apostrophe "s set of labels that are valid targets for a " (:character-literal "continue") " statement.")
    (define (add-continue-labels (c context) (ls (list-set label))) context
      (return (new context (& strict c) (& unchecked c) (& inside-function c) (& private-namespace c) (& open-namespaces c)
                   (& break-labels c) (set+ (& continue-labels c) ls))))
    
    
    (%heading (2 :semantics) "Environments")
    (%text :comment "If the " (:type frame) " is from within a class" :apostrophe "s body, return that class; otherwise, throw an exception.")
    (define (lexical-class (env frame :unused)) class
      (todo))
    
    
    (define (read-variable (env frame :unused) (pn partial-name :unused)) object
      (todo))
    
    (define (write-variable (env frame :unused) (pn partial-name :unused) (new-value object :unused)) void
      (todo))
    
    (define (delete-variable (env frame :unused) (pn partial-name :unused)) boolean
      (todo))
    
    (%text :comment "Return the value of " (:character-literal "this") ". Throw an exception if there is no " (:character-literal "this") " defined.")
    (define (lookup-this (env frame :unused)) object
      (todo))
    
    
    (%heading (3 :semantics) "Compile Environments")
    
    (%heading (3 :semantics) "Dynamic Environments")
    
    (define (make-runtime-frame (env runtime-frame :unused) (f compile-frame :unused)) runtime-frame
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
    (rule :qualifier ((validate (-> (context compile-frame) namespace)))
      (production :qualifier (:identifier) qualifier-identifier
        ((validate c c-env)
         (const a object (read-variable c-env (new partial-name (& open-namespaces c) (name :identifier))))
         (rwhen (not-in a namespace :narrow-false) (throw type-error))
         (return a)))
      (production :qualifier (public) qualifier-public
        ((validate (c :unused) (c-env :unused))
         (return public-namespace)))
      (production :qualifier (private) qualifier-private
        ((validate c (c-env :unused))
         (const p namespace-opt (& private-namespace c))
         (rwhen (in p null :narrow-false)
           (throw syntax-error))
         (return p))))
    
    (rule :simple-qualified-identifier ((name (writable-cell partial-name)) (validate (-> (context compile-frame) void)))
      (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier
        ((validate c (c-env :unused))
         (action<- (name :simple-qualified-identifier 0) (new partial-name (& open-namespaces c) (name :identifier)))))
      (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier
        ((validate c c-env)
         (const q namespace ((validate :qualifier) c c-env))
         (action<- (name :simple-qualified-identifier 0) (new partial-name (list-set q) (name :identifier))))))
    
    (rule :expression-qualified-identifier ((name (writable-cell partial-name)) (validate (-> (context compile-frame) void)))
      (production :expression-qualified-identifier (:paren-expression \:\: :identifier) expression-qualified-identifier-identifier
        ((validate c c-env)
         ((validate :paren-expression) c c-env)
         (const q object (read-reference ((eval :paren-expression) c-env)))
         (rwhen (not-in q namespace :narrow-false) (throw type-error))
         (action<- (name :expression-qualified-identifier 0) (new partial-name (list-set q) (name :identifier))))))
    
    (rule :qualified-identifier ((name (writable-cell partial-name)) (validate (-> (context compile-frame) void)))
      (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple
        ((validate c c-env)
         ((validate :simple-qualified-identifier) c c-env)
         (action<- (name :qualified-identifier 0) (name :simple-qualified-identifier))))
      (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression
        ((validate c c-env)
         ((validate :expression-qualified-identifier) c c-env)
         (action<- (name :qualified-identifier 0) (name :expression-qualified-identifier)))))
    (%print-actions ("Validation and Evaluation" name validate))
    
    
    (%heading 2 "Unit Expressions")
    (rule :unit-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :unit-expression (:paren-list-expression) unit-expression-paren-list-expression
        ((validate c c-env) ((validate :paren-list-expression) c c-env))
        ((eval env) (return ((eval :paren-list-expression) env))))
      (production :unit-expression ($number :no-line-break $string) unit-expression-number-with-unit
        ((validate (c :unused) (c-env :unused)) (todo))
        ((eval (env :unused)) (todo)))
      (production :unit-expression (:unit-expression :no-line-break $string) unit-expression-unit-expression-with-unit
        ((validate (c :unused) (c-env :unused)) (todo))
        ((eval (env :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (%heading 2 "Primary Expressions")
    (rule :primary-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :primary-expression (null) primary-expression-null
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return null)))
      (production :primary-expression (true) primary-expression-true
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return true)))
      (production :primary-expression (false) primary-expression-false
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return false)))
      (production :primary-expression (public) primary-expression-public
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return public-namespace)))
      (production :primary-expression ($number) primary-expression-number
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return (eval $number))))
      (production :primary-expression ($string) primary-expression-string
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return (eval $string))))
      (production :primary-expression (this) primary-expression-this
        ((validate (c :unused) (c-env :unused)) (todo))
        ((eval env) (return (lookup-this env))))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (todo)))
      (production :primary-expression (:unit-expression) primary-expression-unit-expression
        ((validate c c-env) ((validate :unit-expression) c c-env))
        ((eval env) (return ((eval :unit-expression) env))))
      (production :primary-expression (:array-literal) primary-expression-array-literal
        ((validate (c :unused) (c-env :unused)) (todo))
        ((eval (env :unused)) (todo)))
      (production :primary-expression (:object-literal) primary-expression-object-literal
        ((validate (c :unused) (c-env :unused)) (todo))
        ((eval (env :unused)) (todo)))
      (production :primary-expression (:function-expression) primary-expression-function-expression
        ((validate c c-env) ((validate :function-expression) c c-env))
        ((eval env) (return ((eval :function-expression) env)))))
    
    (rule :paren-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :paren-expression (\( (:assignment-expression allow-in) \)) paren-expression-assignment-expression
        (validate (validate :assignment-expression))
        (eval (eval :assignment-expression))))
    
    (rule :paren-list-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)) (eval-as-list (-> (frame) (vector object))))
      (production :paren-list-expression (:paren-expression) paren-list-expression-paren-expression
        ((validate c c-env) ((validate :paren-expression) c c-env))
        ((eval env) (return ((eval :paren-expression) env)))
        ((eval-as-list env)
         (const elt object (read-reference ((eval :paren-expression) env)))
         (return (vector elt))))
      (production :paren-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) paren-list-expression-list-expression
        ((validate c c-env)
         ((validate :list-expression) c c-env)
         ((validate :assignment-expression) c c-env))
        ((eval env)
         (exec (read-reference ((eval :list-expression) env)))
         (return (read-reference ((eval :assignment-expression) env))))
        ((eval-as-list env)
         (const elts (vector object) ((eval-as-list :list-expression) env))
         (const elt object (read-reference ((eval :assignment-expression) env)))
         (return (append elts (vector elt))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Function Expressions")
    (rule :function-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :function-expression (function :function-signature :block) function-expression-anonymous
        ((validate (c :unused) (c-env :unused)) (todo)) ;***** Clear break and continue inside c
        ((eval (env :unused)) (todo)))
      (production :function-expression (function :identifier :function-signature :block) function-expression-named
        ((validate (c :unused) (c-env :unused)) (todo)) ;***** Clear break and continue inside c
        ((eval (env :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Object Literals")
    (production :object-literal (\{ \}) object-literal-empty)
    (production :object-literal (\{ :field-list \}) object-literal-list)
    
    (production :field-list (:literal-field) field-list-one)
    (production :field-list (:field-list \, :literal-field) field-list-more)
    
    (rule :literal-field ((validate (-> (context compile-frame) (list-set string))) (eval (-> (frame) named-argument)))
      (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression
        ((validate c c-env)
         (const names (list-set string) ((validate :field-name) c c-env))
         ((validate :assignment-expression) c c-env)
         (return names))
        ((eval env)
         (const name string ((eval :field-name) env))
         (const value object (read-reference ((eval :assignment-expression) env)))
         (return (new named-argument name value)))))
    
    (rule :field-name ((validate (-> (context compile-frame) (list-set string))) (eval (-> (frame) string)))
      (production :field-name (:identifier) field-name-identifier
        ((validate (c :unused) (c-env :unused)) (return (list-set (name :identifier))))
        ((eval (env :unused)) (return (name :identifier))))
      (production :field-name ($string) field-name-string
        ((validate (c :unused) (c-env :unused)) (return (list-set (eval $string))))
        ((eval (env :unused)) (return (eval $string))))
      (production :field-name ($number) field-name-number
        ((validate (c :unused) (c-env :unused)) (todo))
        ((eval (env :unused)) (todo)))
      (? js2
        (production :field-name (:paren-expression) field-name-paren-expression
          ((validate (c :unused) (c-env :unused)) (todo))
          ((eval (env :unused)) (todo)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Super Expressions")
    (rule :super-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production :super-expression (super) super-expression-super
        ((validate c (c-env :unused))
         (rwhen (in (& private-namespace c) null)
           (throw syntax-error)))
        ((eval env)
         (const this object (lookup-this env))
         (const limit class (lexical-class env))
         (return (new limited-obj-or-ref this limit))))
      (production :super-expression (:full-super-expression) super-expression-full-super-expression
        ((validate c c-env) ((validate :full-super-expression) c c-env))
        ((eval env) (return ((eval :full-super-expression) env)))))
    
    (rule :full-super-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production :full-super-expression (super :paren-expression) full-super-expression-super-paren-expression
        ((validate c c-env)
         (rwhen (in (& private-namespace c) null)
           (throw syntax-error))
         ((validate :paren-expression) c c-env))
        ((eval env)
         (const r obj-or-ref ((eval :paren-expression) env))
         (const limit class (lexical-class env))
         (return (new limited-obj-or-ref r limit)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Postfix Expressions")
    (rule :postfix-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        (validate (validate :attribute-expression))
        (eval (eval :attribute-expression)))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        (validate (validate :full-postfix-expression))
        (eval (eval :full-postfix-expression)))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        (validate (validate :short-new-expression))
        (eval (eval :short-new-expression))))
    
    (rule :postfix-expression-or-super ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production :postfix-expression-or-super (:postfix-expression) postfix-expression-or-super-postfix-expression
        (validate (validate :postfix-expression))
        (eval (eval :postfix-expression)))
      (production :postfix-expression-or-super (:super-expression) postfix-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    
    (rule :attribute-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier
        ((validate c c-env) ((validate :simple-qualified-identifier) c c-env))
        ((eval env) (return (new variable-reference env (name :simple-qualified-identifier)))))
      (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator
        ((validate c c-env)
         ((validate :attribute-expression) c c-env)
         ((validate :member-operator) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :attribute-expression) env)))
         (return ((eval :member-operator) env a))))
      (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call
        ((validate c c-env)
         ((validate :attribute-expression) c c-env)
         ((validate :arguments) c c-env))
        ((eval env)
         (const r obj-or-ref ((eval :attribute-expression) env))
         (const f object (read-reference r))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env))
         (return (unary-dispatch call-table base f args)))))
    
    (rule :full-postfix-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression
        ((validate c c-env) ((validate :primary-expression) c c-env))
        ((eval env) (return ((eval :primary-expression) env))))
      (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier
        ((validate c c-env) ((validate :expression-qualified-identifier) c c-env))
        ((eval env) (return (new variable-reference env (name :expression-qualified-identifier)))))
      (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression
        ((validate c c-env) ((validate :full-new-expression) c c-env))
        ((eval env) (return ((eval :full-new-expression) env))))
      (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator
        ((validate c c-env)
         ((validate :full-postfix-expression) c c-env)
         ((validate :member-operator) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :full-postfix-expression) env)))
         (return ((eval :member-operator) env a))))
      (production :full-postfix-expression (:super-expression :dot-operator) full-postfix-expression-super-dot-operator
        ((validate c c-env)
         ((validate :super-expression) c c-env)
         ((validate :dot-operator) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :super-expression) env)))
         (return ((eval :dot-operator) env a))))
      (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call
        ((validate c c-env)
         ((validate :full-postfix-expression) c c-env)
         ((validate :arguments) c c-env))
        ((eval env)
         (const r obj-or-ref ((eval :full-postfix-expression) env))
         (const f object (read-reference r))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env))
         (return (unary-dispatch call-table base f args))))
      (production :full-postfix-expression (:full-super-expression :arguments) full-postfix-expression-super-call
        ((validate c c-env)
         ((validate :full-super-expression) c c-env)
         ((validate :arguments) c c-env))
        ((eval env)
         (const r obj-or-ref-optional-limit ((eval :full-super-expression) env))
         (const f obj-optional-limit (read-ref-with-limit r))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env))
         (return (unary-dispatch call-table base f args))))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break ++) full-postfix-expression-increment
        ((validate c c-env) ((validate :postfix-expression-or-super) c c-env))
        ((eval env)
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env))
         (const a obj-optional-limit (read-ref-with-limit r))
         (const b object (unary-dispatch increment-table null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return (get-object a))))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break --) full-postfix-expression-decrement
        ((validate c c-env) ((validate :postfix-expression-or-super) c c-env))
        ((eval env)
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env))
         (const a obj-optional-limit (read-ref-with-limit r))
         (const b object (unary-dispatch decrement-table null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return (get-object a)))))
    
    (rule :full-new-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new
        ((validate c c-env)
         ((validate :full-new-subexpression) c c-env)
         ((validate :arguments) c c-env))
        ((eval env)
         (const f object (read-reference ((eval :full-new-subexpression) env)))
         (const args argument-list ((eval :arguments) env))
         (return (unary-dispatch construct-table null f args))))
      (production :full-new-expression (new :full-super-expression :arguments) full-new-expression-super-new
        ((validate c c-env)
         ((validate :full-super-expression) c c-env)
         ((validate :arguments) c c-env))
        ((eval env)
         (const f obj-optional-limit (read-ref-with-limit ((eval :full-super-expression) env)))
         (const args argument-list ((eval :arguments) env))
         (return (unary-dispatch construct-table null f args)))))
    
    (rule :full-new-subexpression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression
        ((validate c c-env) ((validate :primary-expression) c c-env))
        ((eval env) (return ((eval :primary-expression) env))))
      (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier
        ((validate c c-env) ((validate :qualified-identifier) c c-env))
        ((eval env) (return (new variable-reference env (name :qualified-identifier)))))
      (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression
        ((validate c c-env) ((validate :full-new-expression) c c-env))
        ((eval env) (return ((eval :full-new-expression) env))))
      (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator
        ((validate c c-env)
         ((validate :full-new-subexpression) c c-env)
         ((validate :member-operator) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :full-new-subexpression) env)))
         (return ((eval :member-operator) env a))))
      (production :full-new-subexpression (:super-expression :dot-operator) full-new-subexpression-super-dot-operator
        ((validate c c-env)
         ((validate :super-expression) c c-env)
         ((validate :dot-operator) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :super-expression) env)))
         (return ((eval :dot-operator) env a)))))
    
    (rule :short-new-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :short-new-expression (new :short-new-subexpression) short-new-expression-new
        ((validate c c-env) ((validate :short-new-subexpression) c c-env))
        ((eval env)
         (const f object (read-reference ((eval :short-new-subexpression) env)))
         (return (unary-dispatch construct-table null f (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :short-new-expression (new :super-expression) short-new-expression-super-new
        ((validate c c-env) ((validate :super-expression) c c-env))
        ((eval env)
         (const f obj-optional-limit (read-ref-with-limit ((eval :super-expression) env)))
         (return (unary-dispatch construct-table null f (new argument-list (vector-of object) (list-set-of named-argument)))))))
    
    (rule :short-new-subexpression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full
        (validate (validate :full-new-subexpression))
        (eval (eval :full-new-subexpression)))
      (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short
        (validate (validate :short-new-expression))
        (eval (eval :short-new-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Member Operators")
    (rule :member-operator ((validate (-> (context compile-frame) void)) (eval (-> (frame object) obj-or-ref)))
      (production :member-operator (:dot-operator) member-operator-dot-operator
        ((validate c c-env) ((validate :dot-operator) c c-env))
        ((eval env base) (return ((eval :dot-operator) env base))))
      (production :member-operator (\. :paren-expression) member-operator-indirect
        ((validate c c-env) ((validate :paren-expression) c c-env))
        ((eval (env :unused) (base :unused)) (todo))))
    
    (rule :dot-operator ((validate (-> (context compile-frame) void)) (eval (-> (frame obj-optional-limit) obj-or-ref)))
      (production :dot-operator (\. :qualified-identifier) dot-operator-qualified-identifier
        ((validate c c-env) ((validate :qualified-identifier) c c-env))
        ((eval (env :unused) base) (return (new dot-reference base (name :qualified-identifier)))))
      (production :dot-operator (:brackets) dot-operator-brackets
        ((validate c c-env) ((validate :brackets) c c-env))
        ((eval env base)
         (const args argument-list ((eval :brackets) env))
         (return (new bracket-reference base args)))))
    
    (rule :brackets ((validate (-> (context compile-frame) void)) (eval (-> (frame) argument-list)))
      (production :brackets ([ ]) brackets-none
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed
        ((validate c c-env) ((validate :list-expression) c c-env))
        ((eval env)
         (const positional (vector object) ((eval-as-list :list-expression) env))
         (return (new argument-list positional (list-set-of named-argument)))))
      (production :brackets ([ :named-argument-list ]) brackets-named
        ((validate c c-env) (exec ((validate :named-argument-list) c c-env)))
        ((eval env) (return ((eval :named-argument-list) env)))))
    
    (rule :arguments ((validate (-> (context compile-frame) void)) (eval (-> (frame) argument-list)))
      (production :arguments (:paren-expressions) arguments-paren-expressions
        ((validate c c-env) ((validate :paren-expressions) c c-env))
        ((eval env) (return ((eval :paren-expressions) env))))
      (production :arguments (\( :named-argument-list \)) arguments-named
        ((validate c c-env) (exec ((validate :named-argument-list) c c-env)))
        ((eval env) (return ((eval :named-argument-list) env)))))
    
    (rule :paren-expressions ((validate (-> (context compile-frame) void)) (eval (-> (frame) argument-list)))
      (production :paren-expressions (\( \)) paren-expressions-none
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :paren-expressions (:paren-list-expression) paren-expressions-some
        ((validate c c-env) ((validate :paren-list-expression) c c-env))
        ((eval env)
         (const positional (vector object) ((eval-as-list :paren-list-expression) env))
         (return (new argument-list positional (list-set-of named-argument))))))
    
    (rule :named-argument-list ((validate (-> (context compile-frame) (list-set string))) (eval (-> (frame) argument-list)))
      (production :named-argument-list (:literal-field) named-argument-list-one
        ((validate c c-env) (return ((validate :literal-field) c c-env)))
        ((eval env)
         (const na named-argument ((eval :literal-field) env))
         (return (new argument-list (vector-of object) (list-set na)))))
      (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed
        ((validate c c-env)
         ((validate :list-expression) c c-env)
         (return ((validate :literal-field) c c-env)))
        ((eval env)
         (const positional (vector object) ((eval-as-list :list-expression) env))
         (const na named-argument ((eval :literal-field) env))
         (return (new argument-list positional (list-set na)))))
      (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more
        ((validate c c-env)
         (const names1 (list-set string) ((validate :named-argument-list) c c-env))
         (const names2 (list-set string) ((validate :literal-field) c c-env))
         (rwhen (nonempty (set* names1 names2))
           (throw syntax-error))
         (return (set+ names1 names2)))
        ((eval env)
         (const args argument-list ((eval :named-argument-list) env))
         (const na named-argument ((eval :literal-field) env))
         (rwhen (some (& named args) na2 (= (& name na2) (& name na) string))
           (throw argument-mismatch-error))
         (return (new argument-list (& positional args) (set+ (& named args) (list-set na)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Unary Operators")
    (rule :unary-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :unary-expression (:postfix-expression) unary-expression-postfix
        ((validate c c-env) ((validate :postfix-expression) c c-env))
        ((eval env) (return ((eval :postfix-expression) env))))
      (production :unary-expression (delete :postfix-expression) unary-expression-delete
        ((validate c c-env) ((validate :postfix-expression) c c-env))
        ((eval env) (return (delete-reference ((eval :postfix-expression) env)))))
      (production :unary-expression (void :unary-expression) unary-expression-void
        ((validate c c-env) ((validate :unary-expression) c c-env))
        ((eval env)
         (exec (read-reference ((eval :unary-expression) env)))
         (return undefined)))
      (production :unary-expression (typeof :unary-expression) unary-expression-typeof
        ((validate c c-env) ((validate :unary-expression) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :unary-expression) env)))
         (case a
           (:select undefined (return "undefined"))
           (:select (union null prototype) (return "object"))
           (:select boolean (return "boolean"))
           (:select float64 (return "number"))
           (:select string (return "string"))
           (:select namespace (return "namespace"))
           (:select compound-attribute (return "attribute"))
           (:select (union class method-closure) (return "function"))
           (:narrow instance (return (& typeof-string a))))))
      (production :unary-expression (++ :postfix-expression-or-super) unary-expression-increment
        ((validate c c-env) ((validate :postfix-expression-or-super) c c-env))
        ((eval env)
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env))
         (const a obj-optional-limit (read-ref-with-limit r))
         (const b object (unary-dispatch increment-table null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return b)))
      (production :unary-expression (-- :postfix-expression-or-super) unary-expression-decrement
        ((validate c c-env) ((validate :postfix-expression-or-super) c c-env))
        ((eval env)
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env))
         (const a obj-optional-limit (read-ref-with-limit r))
         (const b object (unary-dispatch decrement-table null a (new argument-list (vector-of object) (list-set-of named-argument))))
         (write-reference r b)
         (return b)))
      (production :unary-expression (+ :unary-expression-or-super) unary-expression-plus
        ((validate c c-env) ((validate :unary-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :unary-expression-or-super) env)))
         (return (unary-plus a))))
      (production :unary-expression (- :unary-expression-or-super) unary-expression-minus
        ((validate c c-env) ((validate :unary-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :unary-expression-or-super) env)))
         (return (unary-dispatch minus-table null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (~ :unary-expression-or-super) unary-expression-bitwise-not
        ((validate c c-env) ((validate :unary-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :unary-expression-or-super) env)))
         (return (unary-dispatch bitwise-not-table null a (new argument-list (vector-of object) (list-set-of named-argument))))))
      (production :unary-expression (! :unary-expression) unary-expression-logical-not
        ((validate c c-env) ((validate :unary-expression) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :unary-expression) env)))
         (return (unary-not a)))))
    
    (rule :unary-expression-or-super ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production :unary-expression-or-super (:unary-expression) unary-expression-or-super-unary-expression
        (validate (validate :unary-expression))
        (eval (eval :unary-expression)))
      (production :unary-expression-or-super (:super-expression) unary-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Multiplicative Operators")
    (rule :multiplicative-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        ((validate c c-env) ((validate :unary-expression) c c-env))
        ((eval env) (return ((eval :unary-expression) env))))
      (production :multiplicative-expression (:multiplicative-expression-or-super * :unary-expression-or-super) multiplicative-expression-multiply
        ((validate c c-env)
         ((validate :multiplicative-expression-or-super) c c-env)
         ((validate :unary-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :multiplicative-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :unary-expression-or-super) env)))
         (return (binary-dispatch multiply-table a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super / :unary-expression-or-super) multiplicative-expression-divide
        ((validate c c-env)
         ((validate :multiplicative-expression-or-super) c c-env)
         ((validate :unary-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :multiplicative-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :unary-expression-or-super) env)))
         (return (binary-dispatch divide-table a b))))
      (production :multiplicative-expression (:multiplicative-expression-or-super % :unary-expression-or-super) multiplicative-expression-remainder
        ((validate c c-env)
         ((validate :multiplicative-expression-or-super) c c-env)
         ((validate :unary-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :multiplicative-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :unary-expression-or-super) env)))
         (return (binary-dispatch remainder-table a b)))))
    
    (rule :multiplicative-expression-or-super ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production :multiplicative-expression-or-super (:multiplicative-expression) multiplicative-expression-or-super-multiplicative-expression
        (validate (validate :multiplicative-expression))
        (eval (eval :multiplicative-expression)))
      (production :multiplicative-expression-or-super (:super-expression) multiplicative-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Additive Operators")
    (rule :additive-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        ((validate c c-env) ((validate :multiplicative-expression) c c-env))
        ((eval env) (return ((eval :multiplicative-expression) env))))
      (production :additive-expression (:additive-expression-or-super + :multiplicative-expression-or-super) additive-expression-add
        ((validate c c-env)
         ((validate :additive-expression-or-super) c c-env)
         ((validate :multiplicative-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :additive-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :multiplicative-expression-or-super) env)))
         (return (binary-dispatch add-table a b))))
      (production :additive-expression (:additive-expression-or-super - :multiplicative-expression-or-super) additive-expression-subtract
        ((validate c c-env)
         ((validate :additive-expression-or-super) c c-env)
         ((validate :multiplicative-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :additive-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :multiplicative-expression-or-super) env)))
         (return (binary-dispatch subtract-table a b)))))
    
    (rule :additive-expression-or-super ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production :additive-expression-or-super (:additive-expression) additive-expression-or-super-additive-expression
        (validate (validate :additive-expression))
        (eval (eval :additive-expression)))
      (production :additive-expression-or-super (:super-expression) additive-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Bitwise Shift Operators")
    (rule :shift-expression ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        ((validate c c-env) ((validate :additive-expression) c c-env))
        ((eval env) (return ((eval :additive-expression) env))))
      (production :shift-expression (:shift-expression-or-super << :additive-expression-or-super) shift-expression-left
        ((validate c c-env)
         ((validate :shift-expression-or-super) c c-env)
         ((validate :additive-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :shift-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :additive-expression-or-super) env)))
         (return (binary-dispatch shift-left-table a b))))
      (production :shift-expression (:shift-expression-or-super >> :additive-expression-or-super) shift-expression-right-signed
        ((validate c c-env)
         ((validate :shift-expression-or-super) c c-env)
         ((validate :additive-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :shift-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :additive-expression-or-super) env)))
         (return (binary-dispatch shift-right-table a b))))
      (production :shift-expression (:shift-expression-or-super >>> :additive-expression-or-super) shift-expression-right-unsigned
        ((validate c c-env)
         ((validate :shift-expression-or-super) c c-env)
         ((validate :additive-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :shift-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :additive-expression-or-super) env)))
         (return (binary-dispatch shift-right-unsigned-table a b)))))
    
    (rule :shift-expression-or-super ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production :shift-expression-or-super (:shift-expression) shift-expression-or-super-shift-expression
        (validate (validate :shift-expression))
        (eval (eval :shift-expression)))
      (production :shift-expression-or-super (:super-expression) shift-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Relational Operators")
    (rule (:relational-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
        ((validate c c-env) ((validate :shift-expression) c c-env))
        ((eval env) (return ((eval :shift-expression) env))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) < :shift-expression-or-super) relational-expression-less
        ((validate c c-env)
         ((validate :relational-expression-or-super) c c-env)
         ((validate :shift-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :shift-expression-or-super) env)))
         (return (binary-dispatch less-table a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) > :shift-expression-or-super) relational-expression-greater
        ((validate c c-env)
         ((validate :relational-expression-or-super) c c-env)
         ((validate :shift-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :shift-expression-or-super) env)))
         (return (binary-dispatch less-table b a))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) <= :shift-expression-or-super) relational-expression-less-or-equal
        ((validate c c-env)
         ((validate :relational-expression-or-super) c c-env)
         ((validate :shift-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :shift-expression-or-super) env)))
         (return (binary-dispatch less-or-equal-table a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) >= :shift-expression-or-super) relational-expression-greater-or-equal
        ((validate c c-env)
         ((validate :relational-expression-or-super) c c-env)
         ((validate :shift-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :shift-expression-or-super) env)))
         (return (binary-dispatch less-or-equal-table b a))))
      (production (:relational-expression :beta) ((:relational-expression :beta) is :shift-expression) relational-expression-is
        ((validate c c-env)
         ((validate :relational-expression) c c-env)
         ((validate :shift-expression) c c-env))
        ((eval (env :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((validate c c-env)
         ((validate :relational-expression) c c-env)
         ((validate :shift-expression) c c-env))
        ((eval (env :unused)) (todo)))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression-or-super) relational-expression-in
        ((validate c c-env)
         ((validate :relational-expression) c c-env)
         ((validate :shift-expression-or-super) c c-env))
        ((eval (env :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((validate c c-env)
         ((validate :relational-expression) c c-env)
         ((validate :shift-expression) c c-env))
        ((eval (env :unused)) (todo))))
    
    (rule (:relational-expression-or-super :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production (:relational-expression-or-super :beta) ((:relational-expression :beta)) relational-expression-or-super-relational-expression
        (validate (validate :relational-expression))
        (eval (eval :relational-expression)))
      (production (:relational-expression-or-super :beta) (:super-expression) relational-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Equality Operators")
    (rule (:equality-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        ((validate c c-env) ((validate :relational-expression) c c-env))
        ((eval env) (return ((eval :relational-expression) env))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) == (:relational-expression-or-super :beta)) equality-expression-equal
        ((validate c c-env)
         ((validate :equality-expression-or-super) c c-env)
         ((validate :relational-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :equality-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (return (binary-dispatch equal-table a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) != (:relational-expression-or-super :beta)) equality-expression-not-equal
        ((validate c c-env)
         ((validate :equality-expression-or-super) c c-env)
         ((validate :relational-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :equality-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (return (unary-not (binary-dispatch equal-table a b)))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) === (:relational-expression-or-super :beta)) equality-expression-strict-equal
        ((validate c c-env)
         ((validate :equality-expression-or-super) c c-env)
         ((validate :relational-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :equality-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (return (binary-dispatch strict-equal-table a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) !== (:relational-expression-or-super :beta)) equality-expression-strict-not-equal
        ((validate c c-env)
         ((validate :equality-expression-or-super) c c-env)
         ((validate :relational-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :equality-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :relational-expression-or-super) env)))
         (return (unary-not (binary-dispatch strict-equal-table a b))))))
    
    (rule (:equality-expression-or-super :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production (:equality-expression-or-super :beta) ((:equality-expression :beta)) equality-expression-or-super-equality-expression
        (validate (validate :equality-expression))
        (eval (eval :equality-expression)))
      (production (:equality-expression-or-super :beta) (:super-expression) equality-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        ((validate c c-env) ((validate :equality-expression) c c-env))
        ((eval env) (return ((eval :equality-expression) env))))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression-or-super :beta) & (:equality-expression-or-super :beta)) bitwise-and-expression-and
        ((validate c c-env)
         ((validate :bitwise-and-expression-or-super) c c-env)
         ((validate :equality-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :bitwise-and-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :equality-expression-or-super) env)))
         (return (binary-dispatch bitwise-and-table a b)))))
    
    (rule (:bitwise-xor-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        ((validate c c-env) ((validate :bitwise-and-expression) c c-env))
        ((eval env) (return ((eval :bitwise-and-expression) env))))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression-or-super :beta) ^ (:bitwise-and-expression-or-super :beta)) bitwise-xor-expression-xor
        ((validate c c-env)
         ((validate :bitwise-xor-expression-or-super) c c-env)
         ((validate :bitwise-and-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :bitwise-xor-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :bitwise-and-expression-or-super) env)))
         (return (binary-dispatch bitwise-xor-table a b)))))
    
    (rule (:bitwise-or-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        ((validate c c-env) ((validate :bitwise-xor-expression) c c-env))
        ((eval env) (return ((eval :bitwise-xor-expression) env))))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression-or-super :beta) \| (:bitwise-xor-expression-or-super :beta)) bitwise-or-expression-or
        ((validate c c-env)
         ((validate :bitwise-or-expression-or-super) c c-env)
         ((validate :bitwise-xor-expression-or-super) c c-env))
        ((eval env)
         (const a obj-optional-limit (read-ref-with-limit ((eval :bitwise-or-expression-or-super) env)))
         (const b obj-optional-limit (read-ref-with-limit ((eval :bitwise-xor-expression-or-super) env)))
         (return (binary-dispatch bitwise-or-table a b)))))
    
    
    (rule (:bitwise-and-expression-or-super :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production (:bitwise-and-expression-or-super :beta) ((:bitwise-and-expression :beta)) bitwise-and-expression-or-super-bitwise-and-expression
        (validate (validate :bitwise-and-expression))
        (eval (eval :bitwise-and-expression)))
      (production (:bitwise-and-expression-or-super :beta) (:super-expression) bitwise-and-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    
    (rule (:bitwise-xor-expression-or-super :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production (:bitwise-xor-expression-or-super :beta) ((:bitwise-xor-expression :beta)) bitwise-xor-expression-or-super-bitwise-xor-expression
        (validate (validate :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression)))
      (production (:bitwise-xor-expression-or-super :beta) (:super-expression) bitwise-xor-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    
    (rule (:bitwise-or-expression-or-super :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref-optional-limit)))
      (production (:bitwise-or-expression-or-super :beta) ((:bitwise-or-expression :beta)) bitwise-or-expression-or-super-bitwise-or-expression
        (validate (validate :bitwise-or-expression))
        (eval (eval :bitwise-or-expression)))
      (production (:bitwise-or-expression-or-super :beta) (:super-expression) bitwise-or-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Binary Logical Operators")
    (rule (:logical-and-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        ((validate c c-env) ((validate :bitwise-or-expression) c c-env))
        ((eval env) (return ((eval :bitwise-or-expression) env))))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((validate c c-env)
         ((validate :logical-and-expression) c c-env)
         ((validate :bitwise-or-expression) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :logical-and-expression) env)))
         (if (to-boolean a)
           (return (read-reference ((eval :bitwise-or-expression) env)))
           (return a)))))
    
    (rule (:logical-xor-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        ((validate c c-env) ((validate :logical-and-expression) c c-env))
        ((eval env) (return ((eval :logical-and-expression) env))))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((validate c c-env)
         ((validate :logical-xor-expression) c c-env)
         ((validate :logical-and-expression) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :logical-xor-expression) env)))
         (const b object (read-reference ((eval :logical-and-expression) env)))
         (const ab boolean (to-boolean a))
         (const bb boolean (to-boolean b))
         (return (xor ab bb)))))
    
    (rule (:logical-or-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        ((validate c c-env) ((validate :logical-xor-expression) c c-env))
        ((eval env) (return ((eval :logical-xor-expression) env))))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((validate c c-env)
         ((validate :logical-or-expression) c c-env)
         ((validate :logical-xor-expression) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :logical-or-expression) env)))
         (if (to-boolean a) 
           (return a)
           (return (read-reference ((eval :logical-xor-expression) env)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Conditional Operator")
    (rule (:conditional-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        ((validate c c-env) ((validate :logical-or-expression) c c-env))
        ((eval env) (return ((eval :logical-or-expression) env))))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((validate c c-env)
         ((validate :logical-or-expression) c c-env)
         ((validate :assignment-expression 1) c c-env)
         ((validate :assignment-expression 2) c c-env))
        ((eval env)
         (if (to-boolean (read-reference ((eval :logical-or-expression) env)))
           (return (read-reference ((eval :assignment-expression 1) env)))
           (return (read-reference ((eval :assignment-expression 2) env)))))))
    
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or)
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Assignment Operators")
    (rule (:assignment-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        ((validate c c-env) ((validate :conditional-expression) c c-env))
        ((eval env) (return ((eval :conditional-expression) env))))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((validate c c-env)
         ((validate :postfix-expression) c c-env)
         ((validate :assignment-expression) c c-env))
        ((eval env)
         (const r obj-or-ref ((eval :postfix-expression) env))
         (const a object (read-reference ((eval :assignment-expression) env)))
         (write-reference r a)
         (return a)))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((validate c c-env)
         ((validate :postfix-expression-or-super) c c-env)
         ((validate :assignment-expression) c c-env))
        ((eval env)
         (return (eval-assignment-op (table :compound-assignment) (eval :postfix-expression-or-super) (eval :assignment-expression) env))))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment :super-expression) assignment-expression-compound-super
        ((validate c c-env)
         ((validate :postfix-expression-or-super) c c-env)
         ((validate :super-expression) c c-env))
        ((eval env)
         (return (eval-assignment-op (table :compound-assignment) (eval :postfix-expression-or-super) (eval :super-expression) env))))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((validate c c-env)
         ((validate :postfix-expression) c c-env)
         ((validate :assignment-expression) c c-env))
        ((eval env)
         (const r-left obj-or-ref ((eval :postfix-expression) env))
         (const o-left object (read-reference r-left))
         (const b-left boolean (to-boolean o-left))
         (var result object o-left)
         (case (operator :logical-assignment)
           (:select (tag and-eq)
             (when b-left
               (<- result (read-reference ((eval :assignment-expression) env)))))
           (:select (tag xor-eq)
             (const b-right boolean (to-boolean (read-reference ((eval :assignment-expression) env))))
             (<- result (xor b-left b-right)))
           (:select (tag or-eq)
             (when (not b-left)
               (<- result (read-reference ((eval :assignment-expression) env))))))
         (write-reference r-left result)
         (return result))))
    
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
    
    (rule :logical-assignment ((operator (tag and-eq xor-eq or-eq)))
      (production :logical-assignment (&&=) logical-assignment-logical-and (operator and-eq))
      (production :logical-assignment (^^=) logical-assignment-logical-xor (operator xor-eq))
      (production :logical-assignment (\|\|=) logical-assignment-logical-or (operator or-eq)))
    
    (deftag and-eq)
    (deftag xor-eq)
    (deftag or-eq)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (eval-assignment-op (table (list-set binary-method))
                                (left-eval (-> (frame) obj-or-ref-optional-limit))
                                (right-eval (-> (frame) obj-or-ref-optional-limit))
                                (env frame)) obj-or-ref
      (const r-left obj-or-ref-optional-limit (left-eval env))
      (const o-left obj-optional-limit (read-ref-with-limit r-left))
      (const o-right obj-optional-limit (read-ref-with-limit (right-eval env)))
      (const result object (binary-dispatch table o-left o-right))
      (write-reference r-left result)
      (return result))
    
    
    (%heading 2 "Comma Expressions")
    (rule (:list-expression :beta) ((validate (-> (context compile-frame) void)) (eval (-> (frame) obj-or-ref)) (eval-as-list (-> (frame) (vector object))))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        ((validate c c-env) ((validate :assignment-expression) c c-env))
        ((eval env) (return ((eval :assignment-expression) env)))
        ((eval-as-list env)
         (const elt object (read-reference ((eval :assignment-expression) env)))
         (return (vector elt))))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((validate c c-env)
         ((validate :list-expression) c c-env)
         ((validate :assignment-expression) c c-env))
        ((eval env)
         (exec (read-reference ((eval :list-expression) env)))
         (return (read-reference ((eval :assignment-expression) env))))
        ((eval-as-list env)
         (const elts (vector object) ((eval-as-list :list-expression) env))
         (const elt object (read-reference ((eval :assignment-expression) env)))
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
    
    (rule (:statement :omega) ((validate (-> (context compile-frame (list-set label)) void)) (eval (-> (runtime-frame object) object)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        ((validate c c-env (sl :unused)) ((validate :expression-statement) c c-env))
        ((eval r-env (d :unused)) (return ((eval :expression-statement) r-env))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((validate c c-env (sl :unused)) ((validate :super-statement) c c-env))
        ((eval r-env (d :unused)) (return ((eval :super-statement) r-env))))
      (production (:statement :omega) (:block) statement-block
        ((validate c c-env (sl :unused)) ((validate :block) c c-env))
        ((eval r-env d) (return ((eval :block) r-env d))))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        ((validate c c-env sl) ((validate :labeled-statement) c c-env sl))
        ((eval r-env d) (return ((eval :labeled-statement) r-env d))))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        ((validate c c-env (sl :unused)) ((validate :if-statement) c c-env))
        ((eval r-env d) (return ((eval :if-statement) r-env d))))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((validate (c :unused) (c-env :unused) (sl :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((validate c c-env sl) ((validate :do-statement) c c-env sl))
        ((eval r-env d) (return ((eval :do-statement) r-env d))))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((validate c c-env sl) ((validate :while-statement) c c-env sl))
        ((eval r-env d) (return ((eval :while-statement) r-env d))))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((validate (c :unused) (c-env :unused) (sl :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((validate (c :unused) (c-env :unused) (sl :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        ((validate c (c-env :unused) (sl :unused)) ((validate :continue-statement) c))
        ((eval r-env d) (return ((eval :continue-statement) r-env d))))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        ((validate c (c-env :unused) (sl :unused)) ((validate :break-statement) c))
        ((eval r-env d) (return ((eval :break-statement) r-env d))))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        ((validate c c-env (sl :unused)) ((validate :return-statement) c c-env))
        ((eval r-env (d :unused)) (return ((eval :return-statement) r-env))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((validate c c-env (sl :unused)) ((validate :throw-statement) c c-env))
        ((eval r-env (d :unused)) (return ((eval :throw-statement) r-env))))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((validate (c :unused) (c-env :unused) (sl :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo))))
    
    
    (rule (:substatement :omega) ((enabled (writable-cell boolean)) (validate (-> (context compile-frame (list-set label)) void))
                                  (eval (-> (runtime-frame object) object)))
      (production (:substatement :omega) (:empty-statement) substatement-empty-statement
        ((validate (c :unused) (c-env :unused) (sl :unused)))
        ((eval (r-env :unused) d) (return d)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        ((validate c c-env sl) ((validate :statement) c c-env sl))
        ((eval r-env d) (return ((eval :statement) r-env d))))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((validate (c :unused) (c-env :unused) (sl :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:substatement :omega) (:attributes :no-line-break { :substatements }) substatement-annotated-group
        ((validate c c-env (sl :unused))
         ((validate :attributes) c c-env)
         (const a attribute ((eval :attributes) c-env))
         (rwhen (not-in a boolean :narrow-false)
           (throw type-error))
         (action<- (enabled :substatement 0) a)
         (when a
           ((validate :substatements) c c-env)))
        ((eval r-env d)
         (if (enabled :substatement 0)
           (return ((eval :substatements) r-env d))
           (return d)))))
    
    
    (rule :substatements ((validate (-> (context compile-frame) void)) (eval (-> (runtime-frame object) object)))
      (production :substatements () substatements-none
        ((validate (c :unused) (c-env :unused)))
        ((eval (r-env :unused) d) (return d)))
      (production :substatements (:substatements-prefix (:substatement abbrev)) substatements-more
        ((validate c c-env)
         ((validate :substatements-prefix) c c-env)
         ((validate :substatement) c c-env (list-set-of label)))
        ((eval r-env d)
         (const o object ((eval :substatements-prefix) r-env d))
         (return ((eval :substatement) r-env o)))))
    
    (rule :substatements-prefix ((validate (-> (context compile-frame) void)) (eval (-> (runtime-frame object) object)))
      (production :substatements-prefix () substatements-prefix-none
        ((validate (c :unused) (c-env :unused)))
        ((eval (r-env :unused) d) (return d)))
      (production :substatements-prefix (:substatements-prefix (:substatement full)) substatements-prefix-more
        ((validate c c-env)
         ((validate :substatements-prefix) c c-env)
         ((validate :substatement) c c-env (list-set-of label)))
        ((eval r-env d)
         (const o object ((eval :substatements-prefix) r-env d))
         (return ((eval :substatement) r-env o)))))
    
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon no-short-if) () semicolon-no-short-if)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%heading 2 "Expression Statement")
    (rule :expression-statement ((validate (-> (context compile-frame) void)) (eval (-> (runtime-frame) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        ((validate c c-env) ((validate :list-expression) c c-env))
        ((eval r-env) (return (read-reference ((eval :list-expression) r-env))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Super Statement")
    (rule :super-statement ((validate (-> (context compile-frame) void)) (eval (-> (runtime-frame) object)))
      (production :super-statement (super :arguments) super-statement-super-arguments
        ((validate (c :unused) (c-env :unused)) (todo))
        ((eval (r-env :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Block Statement")
    (rule :block ((frame (writable-cell compile-frame)) (validate (-> (context compile-frame) void)) (eval (-> (runtime-frame object) object)))
      (production :block ({ :directives }) block-directives
        ((validate c c-env)
         (const f block-compile-frame (new block-compile-frame c-env (vector-of compile-or-anti-binding)))
         (action<- (frame :block 0) f)
         (exec ((validate :directives) c f true)))
        ((eval r-env d)
         (const inner-env runtime-frame (make-runtime-frame r-env (frame :block 0)))
         (return ((eval :directives) inner-env d)))))
    (%print-actions ("Validation" attribute validate) ("Evaluation" eval))
    
    
    (%heading 2 "Labeled Statements")
    (rule (:labeled-statement :omega) ((validate (-> (context compile-frame (list-set label)) void)) (eval (-> (runtime-frame object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((validate c c-env sl)
         (const name string (name :identifier))
         (rwhen (set-in name (& break-labels c))
           (throw syntax-error))
         (const c2 context (add-break-label c name))
         ((validate :substatement) c2 c-env (set+ sl (list-set-of label name))))
        ((eval r-env d)
         (catch ((return ((eval :substatement) r-env d)))
           (x) (if (and (in x break :narrow-true) (= (& label x) (name :identifier) label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "If Statement")
    (rule (:if-statement :omega) ((validate (-> (context compile-frame) void)) (eval (-> (runtime-frame object) object)))
      (production (:if-statement abbrev) (if :paren-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((validate c c-env)
         ((validate :paren-list-expression) c c-env)
         ((validate :substatement) c c-env (list-set-of label)))
        ((eval r-env d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) r-env)))
           (return ((eval :substatement) r-env d))
           (return d))))
      (production (:if-statement full) (if :paren-list-expression (:substatement full)) if-statement-if-then-full
        ((validate c c-env)
         ((validate :paren-list-expression) c c-env)
         ((validate :substatement) c c-env (list-set-of label)))
        ((eval r-env d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) r-env)))
           (return ((eval :substatement) r-env d))
           (return d))))
      (production (:if-statement :omega) (if :paren-list-expression (:substatement no-short-if) else (:substatement :omega))
                  if-statement-if-then-else
        ((validate c c-env)
         ((validate :paren-list-expression) c c-env)
         ((validate :substatement 1) c c-env (list-set-of label))
         ((validate :substatement 2) c c-env (list-set-of label)))
        ((eval r-env d)
         (if (to-boolean (read-reference ((eval :paren-list-expression) r-env)))
           (return ((eval :substatement 1) r-env d))
           (return ((eval :substatement 2) r-env d))))))
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
    (rule :do-statement ((labels (writable-cell (list-set label))) (validate (-> (context compile-frame (list-set label)) void))
                         (eval (-> (runtime-frame object) object)))
      (production :do-statement (do (:substatement abbrev) while :paren-list-expression) do-statement-do-while
        ((validate c c-env sl)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :do-statement 0) continue-labels)
         (const c2 context (add-break-label c default))
         (const c3 context (add-continue-labels c2 continue-labels))
         ((validate :substatement) c3 c-env (list-set-of label))
         ((validate :paren-list-expression) c c-env))
        ((eval r-env d)
         (catch ((var d1 object d)
                 (while true
                   (catch ((<- d1 ((eval :substatement) r-env d1)))
                     (x) (if (and (in x continue :narrow-true) (set-in (& label x) (labels :do-statement 0)))
                           (<- d1 (& value x))
                           (throw x)))
                   (rwhen (not (to-boolean (read-reference ((eval :paren-list-expression) r-env))))
                     (return d1))))
           (x) (if (and (in x break :narrow-true) (= (& label x) default label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" labels validate) ("Evaluation" eval))
    
    
    (%heading 2 "While Statement")
    (rule (:while-statement :omega) ((labels (writable-cell (list-set label))) (validate (-> (context compile-frame (list-set label)) void))
                                     (eval (-> (runtime-frame object) object)))
      (production (:while-statement :omega) (while :paren-list-expression (:substatement :omega)) while-statement-while
        ((validate c c-env sl)
         ((validate :paren-list-expression) c c-env)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :while-statement 0) continue-labels)
         (const c2 context (add-break-label c default))
         (const c3 context (add-continue-labels c2 continue-labels))
         ((validate :substatement) c3 c-env (list-set-of label)))
        ((eval r-env d)
         (catch ((var d1 object d)
                 (while (to-boolean (read-reference ((eval :paren-list-expression) r-env)))
                   (catch ((<- d1 ((eval :substatement) r-env d1)))
                     (x) (if (and (in x continue :narrow-true) (set-in (& label x) (labels :while-statement 0)))
                           (<- d1 (& value x))
                           (throw x))))
                 (return d1))
           (x) (if (and (in x break :narrow-true) (= (& label x) default label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" labels validate) ("Evaluation" eval))
    
    
    (%heading 2 "For Statements")
    (production (:for-statement :omega) (for \( :for-initialiser \; :optional-expression \; :optional-expression \)
                                             (:substatement :omega)) for-statement-c-style)
    (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:substatement :omega)) for-statement-in)
    
    (production :for-initialiser () for-initialiser-empty)
    (production :for-initialiser ((:list-expression no-in)) for-initialiser-expression)
    (production :for-initialiser (:variable-definition-kind (:variable-binding-list no-in)) for-initialiser-variable-definition)
    (production :for-initialiser (:attributes :no-line-break :variable-definition-kind (:variable-binding-list no-in)) for-initialiser-attribute-variable-definition)
    
    (production :for-in-binding (:postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    (production :for-in-binding (:attributes :no-line-break :variable-definition-kind (:variable-binding no-in)) for-in-binding-attribute-variable-definition)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "With Statement")
    (production (:with-statement :omega) (with :paren-list-expression (:substatement :omega)) with-statement-with)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Continue and Break Statements")
    (rule :continue-statement ((validate (-> (context) void)) (eval (-> (runtime-frame object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((validate c)
         (rwhen (set-not-in default (& continue-labels c))
           (throw syntax-error)))
        ((eval (r-env :unused) d) (throw (new continue d default))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((validate c)
         (rwhen (set-not-in (name :identifier) (& continue-labels c))
           (throw syntax-error)))
        ((eval (r-env :unused) d) (throw (new continue d (name :identifier))))))
    
    (rule :break-statement ((validate (-> (context) void)) (eval (-> (runtime-frame object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((validate c)
         (rwhen (set-not-in default (& break-labels c))
           (throw syntax-error)))
        ((eval (r-env :unused) d) (throw (new break d default))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((validate c)
         (rwhen (set-not-in (name :identifier) (& break-labels c))
           (throw syntax-error)))
        ((eval (r-env :unused) d) (throw (new break d (name :identifier))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Return Statement")
    (rule :return-statement ((validate (-> (context compile-frame) void)) (eval (-> (runtime-frame) object)))
      (production :return-statement (return) return-statement-default
        ((validate c (c-env :unused))
         (rwhen (not (& inside-function c))
           (throw syntax-error)))
        ((eval (r-env :unused)) (throw (new returned-value undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((validate c c-env)
         (rwhen (not (& inside-function c))
           (throw syntax-error))
         ((validate :list-expression) c c-env))
        ((eval r-env)
         (const a object (read-reference ((eval :list-expression) r-env)))
         (throw (new returned-value a)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Throw Statement")
    (rule :throw-statement ((validate (-> (context compile-frame) void)) (eval (-> (runtime-frame) object)))
      (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw
        (validate (validate :list-expression))
        ((eval r-env)
         (const a object (read-reference ((eval :list-expression) r-env)))
         (throw (new thrown-value a)))))
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
    (rule (:directive :omega_2) ((enabled (writable-cell boolean)) (validate (-> (context compile-frame attribute-not-false) context))
                                 (eval (-> (runtime-frame object) object)))
      (production (:directive :omega_2) (:empty-statement) directive-empty-statement
        ((validate c (c-env :unused) (a :unused)) (return c))
        ((eval (r-env :unused) d) (return d)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        ((validate c c-env a)
         (rwhen (not-in a true-type)
           (throw syntax-error))
         ((validate :statement) c c-env (list-set-of label))
         (return c))
        ((eval r-env d) (return ((eval :statement) r-env d))))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((validate c c-env a) (return ((validate :annotatable-directive) c c-env a false)))
        ((eval r-env d) (return ((eval :annotatable-directive) r-env d))))
      (production (:directive :omega_2) (:attributes :no-line-break (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((validate c c-env a)
         ((validate :attributes) c c-env)
         (const a2 attribute ((eval :attributes) c-env))
         (const a3 attribute (combine-attributes a a2))
         (action<- (enabled :directive 0) (not-in a3 false-type))
         (if (not-in a3 false-type :narrow-true)
           (return ((validate :annotatable-directive) c c-env a3 true))
           (return c)))
        ((eval r-env d)
         (if (enabled :directive 0)
           (return ((eval :annotatable-directive) r-env d))
           (return d))))
      (production (:directive :omega_2) (:attributes :no-line-break { :directives }) directive-annotated-group
        ((validate c c-env a)
         ((validate :attributes) c c-env)
         (const a2 attribute ((eval :attributes) c-env))
         (const a3 attribute (combine-attributes a a2))
         (action<- (enabled :directive 0) (not-in a3 false-type))
         (rwhen (in a3 false-type :narrow-false)
           (return c))
         (return ((validate :directives) c c-env a3)))
        ((eval r-env d)
         (if (enabled :directive 0)
           (return ((eval :directives) r-env d))
           (return d))))
      (production (:directive :omega_2) (:package-definition) directive-package-definition
        ((validate (c :unused) (c-env :unused) a) (if (in a true-type) (todo) (throw syntax-error)))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((validate (c :unused) (c-env :unused) a) (if (in a true-type) (todo) (throw syntax-error)))
          ((eval (r-env :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((validate (c :unused) (c-env :unused) (a :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo))))
    
    (rule (:annotatable-directive :omega_2) ((validate (-> (context compile-frame attribute-not-false boolean) context)) (eval (-> (runtime-frame object) object)))
      (production (:annotatable-directive :omega_2) (:export-definition (:semicolon :omega_2)) annotatable-directive-export-definition
        ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:variable-definition (:semicolon :omega_2)) annotatable-directive-variable-definition
        ((validate c c-env a has-attributes)
         ((validate :variable-definition) c c-env a has-attributes)
         (return c))
        ((eval r-env d) (return ((eval :variable-definition) r-env d))))
      (production (:annotatable-directive :omega_2) ((:function-definition :omega_2)) annotatable-directive-function-definition
        ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) ((:class-definition :omega_2)) annotatable-directive-class-definition
        ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:namespace-definition (:semicolon :omega_2)) annotatable-directive-namespace-definition
        ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (? js2
        (production (:annotatable-directive :omega_2) ((:interface-definition :omega_2)) annotatable-directive-interface-definition
          ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
          ((eval (r-env :unused) (d :unused)) (todo))))
      (production (:annotatable-directive :omega_2) (:import-directive (:semicolon :omega_2)) annotatable-directive-import-directive
        ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:use-directive (:semicolon :omega_2)) annotatable-directive-use-directive
        ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo))))
    
    
    (rule :directives ((validate (-> (context compile-frame attribute-not-false) context)) (eval (-> (runtime-frame object) object)))
      (production :directives () directives-none
        ((validate c (c-env :unused) (a :unused)) (return c))
        ((eval (r-env :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((validate c c-env a)
         (const c2 context ((validate :directives-prefix) c c-env a))
         (return ((validate :directive) c2 c-env a)))
        ((eval r-env d)
         (const o object ((eval :directives-prefix) r-env d))
         (return ((eval :directive) r-env o)))))
    
    (rule :directives-prefix ((validate (-> (context compile-frame attribute-not-false) context)) (eval (-> (runtime-frame object) object)))
      (production :directives-prefix () directives-prefix-none
        ((validate c (c-env :unused) (a :unused)) (return c))
        ((eval (r-env :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((validate c c-env a)
         (const c2 context ((validate :directives-prefix) c c-env a))
         (return ((validate :directive) c2 c-env a)))
        ((eval r-env d)
         (const o object ((eval :directives-prefix) r-env d))
         (return ((eval :directive) r-env o)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Attributes")
    (rule :attributes ((validate (-> (context compile-frame) void)) (eval (-> (frame) attribute)))
      (production :attributes (:attribute) attributes-one
        ((validate c c-env) ((validate :attribute) c c-env))
        ((eval env) (return ((eval :attribute) env))))
      (production :attributes (:attribute :no-line-break :attributes) attributes-more
        ((validate c c-env)
         ((validate :attribute) c c-env)
         ((validate :attributes) c c-env))
        ((eval env)
         (const a attribute ((eval :attribute) env))
         (rwhen (in a false-type :narrow-false)
           (return false))
         (const b attribute ((eval :attributes) env))
         (return (combine-attributes a b)))))
    
    
    (rule :attribute ((private-namespace (writable-cell namespace)) (validate (-> (context compile-frame) void)) (eval (-> (frame) attribute)))
      (production :attribute (:attribute-expression) attribute-attribute-expression
        ((validate c c-env) ((validate :attribute-expression) c c-env))
        ((eval env)
         (const a object (read-reference ((eval :attribute-expression) env)))
         (rwhen (not-in a attribute :narrow-false)
           (throw type-error))
         (return a)))
      (production :attribute (abstract) attribute-abstract
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return (new compound-attribute (list-set-of namespace) null false false false abstract null false false))))
      (production :attribute (final) attribute-final
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return (new compound-attribute (list-set-of namespace) null false false false final null false false))))
      (production :attribute (private) attribute-private
        ((validate c (c-env :unused))
         (const p namespace-opt (& private-namespace c))
         (rwhen (in p null :narrow-false)
           (throw syntax-error))
         (action<- (private-namespace :attribute 0) p))
        ((eval (env :unused)) (return (private-namespace :attribute 0))))
      (production :attribute (public) attribute-public
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return public-namespace)))
      (production :attribute (static) attribute-static
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return (new compound-attribute (list-set-of namespace) null false false false static null false false))))
      (production :attribute (true) attribute-true
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return true)))
      (production :attribute (false) attribute-false
        ((validate (c :unused) (c-env :unused)))
        ((eval (env :unused)) (return false))))
    (%print-actions ("Validation" private-namespace validate) ("Evaluation" eval))
    
    
    
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
    (rule :variable-definition ((validate (-> (context compile-frame attribute-not-false boolean) void)) (eval (-> (runtime-frame object) object)))
      (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition
        ((validate (c :unused) (c-env :unused) (a :unused) (has-attributes :unused)) (todo))
        ((eval (r-env :unused) (d :unused)) (todo))))
    
    (production :variable-definition-kind (var) variable-definition-kind-var)
    (production :variable-definition-kind (const) variable-definition-kind-const)
    
    (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one)
    (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more)
    
    (production (:variable-binding :beta) ((:typed-identifier :beta)) variable-binding-simple)
    (production (:variable-binding :beta) ((:typed-identifier :beta) = (:variable-initialiser :beta)) variable-binding-initialised)
    
    (production (:variable-initialiser :beta) ((:assignment-expression :beta)) variable-initialiser-assignment-expression)
    (production (:variable-initialiser :beta) (abstract) variable-initialiser-abstract)
    (production (:variable-initialiser :beta) (final) variable-initialiser-final)
    (production (:variable-initialiser :beta) (private) variable-initialiser-private)
    (production (:variable-initialiser :beta) (static) variable-initialiser-static)
    (production (:variable-initialiser :beta) (:attribute :no-line-break :attributes) variable-initialiser-multiple-attributes)
    
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
    ;***** Clear break and continue inside validate
    
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
    ;***** Clear break and continue inside validate
    
    (production :inheritance () inheritance-none)
    (production :inheritance (extends (:type-expression allow-in)) inheritance-extends)
    (? js2
      (production :inheritance (implements :type-expression-list) inheritance-implements)
      (production :inheritance (extends (:type-expression allow-in) implements :type-expression-list) inheritance-extends-implements)
      
      (%heading 2 "Interface Definition")
      (production (:interface-definition :omega_2) (interface :identifier :extends-list :block) interface-definition-definition)
      (production (:interface-definition :omega_2) (interface :identifier (:semicolon :omega_2)) interface-definition-declaration)
      ;***** Clear break and continue inside validate
      
      (production :extends-list () extends-list-none)
      (production :extends-list (extends :type-expression-list) extends-list-one)
      
      (production :type-expression-list ((:type-expression allow-in)) type-expression-list-one)
      (production :type-expression-list (:type-expression-list \, (:type-expression allow-in)) type-expression-list-more))
    
    
    (%heading 2 "Namespace Definition")
    (production :namespace-definition (namespace :identifier) namespace-definition-normal)
    
    
    (%heading 2 "Package Definition")
    (production :package-definition (package :block) package-definition-anonymous)
    (production :package-definition (package :package-name :block) package-definition-named)
    ;***** Clear break and continue inside validate
    
    (production :package-name (:identifier) package-name-one)
    (production :package-name (:package-name \. :identifier) package-name-more)
    
    
    (%heading 1 "Programs")
    (rule :program ((eval-program object))
      (production :program (:directives) program-directives
        (eval-program
         (begin
          (exec ((validate :directives) initial-context initial-compile-frame true))
          (return ((eval :directives) initial-runtime-frame undefined))))))
    
    
    
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
      (return (binary-dispatch add-table x 1.0)))
    
    (define (decrement-object (this object :unused) (a object) (args argument-list :unused)) object
      (const x object (unary-plus a))
      (return (binary-dispatch subtract-table x 1.0)))
    
    (define (call-object (this object) (a object) (args argument-list)) object
      (case a
        (:select (union undefined null boolean float64 string namespace compound-attribute prototype) (throw type-error))
        (:narrow (union class instance) (return ((& call a) this args)))
        (:narrow method-closure (return (call-object (& this a) (& f (& method a)) args)))))
    
    (define (construct-object (this object) (a object) (args argument-list)) object
      (case a
        (:select (union undefined null boolean float64 string namespace compound-attribute method-closure prototype) (throw type-error))
        (:narrow (union class instance) (return ((& construct a) this args)))))
    
    (define (bracket-read-object (this object :unused) (a object) (args argument-list)) object
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0)))
      (return (read-qualified-property a (new qualified-name public-namespace name) true)))
    
    (define (bracket-write-object (this object :unused) (a object) (args argument-list)) object
      (rwhen (or (/= (length (& positional args)) 2) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const new-value object (nth (& positional args) 0))
      (const name string (to-string (nth (& positional args) 1)))
      (write-qualified-property a (new qualified-name public-namespace name) true new-value)
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
            (:select (union undefined null namespace compound-attribute class method-closure prototype instance) (return false))
            (:select (union boolean string float64) (return (= (float64-compare a (to-number bp)) equal order)))))
        (:narrow string
          (const bp object (to-primitive b null))
          (case bp
            (:select (union undefined null namespace compound-attribute class method-closure prototype instance) (return false))
            (:select (union boolean float64) (return (= (float64-compare (to-number a) (to-number bp)) equal order)))
            (:narrow string (return (= a bp string)))))
        (:select (union namespace compound-attribute class method-closure prototype instance)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace compound-attribute class method-closure prototype instance) (return (strict-equal-objects a b)))
            (:select (union boolean float64 string)
              (const ap object (to-primitive a null))
              (case ap
                (:select (union undefined null namespace compound-attribute class method-closure prototype instance) (return false))
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
      (setf (svref bins 2) (list '&&= '^^ '^^= '\|\|=))
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
