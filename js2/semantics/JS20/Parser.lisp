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
    (deftag compile-expression-error)
    (deftag reference-error)
    (deftag uninitialised-error)
    (deftag type-error)
    (deftag property-not-found-error)
    (deftag argument-mismatch-error)
    (deftype semantic-error (tag syntax-error compile-expression-error reference-error uninitialised-error type-error property-not-found-error
                                 argument-mismatch-error))
    
    (deftuple break (value object) (label label))
    (deftuple continue (value object) (label label))
    (deftuple returned-value (value object))
    (deftuple thrown-value (value object))
    (deftype early-exit (union break continue returned-value thrown-value))
    
    (deftype semantic-exception (union early-exit semantic-error))
    
    
    (%heading (2 :semantics) "Objects")
    (deftype object (union undefined null boolean float64 string namespace compound-attribute class method-closure prototype instance))
    
    (deftag none)
    (deftag none-yet)
    (deftag ok)
    (deftag unknown)
    
    
    (%heading (3 :semantics) "Undefined")
    (deftag undefined)
    (deftype undefined (tag undefined))
    
    
    (%heading (3 :semantics) "Null")
    (deftag null)
    (deftype null (tag null))
    
    
    (%heading (3 :semantics) "Namespaces")
    (defrecord namespace (name string))
    
    (define public-namespace namespace (new namespace "public"))
    
    
    (%heading (4 :semantics) "Qualified Names")
    (deftuple qualified-name (namespace namespace) (id string))
    (deftype qualified-name-opt (union (tag none) qualified-name))
    
    (deftype multiname (list-set qualified-name))
    
    
    (%heading (3 :semantics) "Attributes")
    (deftag static)
    (deftag constructor)
    (deftag operator)
    (deftag abstract)
    (deftag virtual)
    (deftag final)
    (deftype member-modifier (tag none static constructor operator abstract virtual final))
    
    (deftag may-override)
    (deftag override)
    (deftype override-modifier (tag none may-override override))
    
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
    (deftype attribute-opt-not-false (union (tag none true) namespace compound-attribute))
    
    
    (%heading (3 :semantics) "Classes")
    (defrecord class
      (super class-opt)
      (prototype object)
      (read-bindings (list-set class-binding) :var)
      (write-bindings (list-set class-binding) :var)
      (private-namespace namespace)
      (dynamic boolean)
      (primitive boolean)
      (call invoker)
      (construct invoker))
    (deftype class-opt (union (tag none) class))
    
    (define (make-built-in-class (superclass class-opt) (dynamic boolean) (primitive boolean)) class
      (function (call (this object :unused) (args argument-list :unused) (mode mode :unused)) object
        (todo))
      (function (construct (this object :unused) (args argument-list :unused) (mode mode :unused)) object
        (todo))
      (const private-namespace namespace (new namespace "private"))
      (return (new class superclass null (list-set-of class-binding) (list-set-of class-binding) private-namespace
                   dynamic primitive call construct)))
    
    (define object-class class (make-built-in-class none false true))
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
      (if (in s (tag none) :narrow-false)
        (return (vector c))
        (return (append (ancestors s) (vector c)))))
    
    (%text :comment "Return " (:tag true) " if " (:local c) " is " (:local d) " or an ancestor of " (:local d) ".")
    (define (is-ancestor (c class) (d class)) boolean
      (cond
       ((= c d class) (return true))
       (nil (const s class-opt (& super d))
            (rwhen (in s (tag none) :narrow-false)
              (return false))
            (return (is-ancestor c s)))))
    
    (%text :comment "Return " (:tag true) " if " (:local c) " is an ancestor of " (:local d) " other than " (:local d) " itself.")
    (define (is-proper-ancestor (c class) (d class)) boolean
      (return (and (is-ancestor c d) (/= c d class))))
    
    
    (%heading (4 :semantics) "Members")
    (deftag indexable)
    (deftag enumerable)
    (deftype visibility-modifier (tag none indexable enumerable))
    
    (deftuple class-binding 
      (qname qualified-name)
      (member member)
      (visibility-modifier visibility-modifier))
    
    (deftype instance-member (union instance-variable instance-method instance-accessor))
    (deftype static-member (union variable static-method static-accessor))
    (deftype member (union instance-member static-member))
    (deftype member-opt (union (tag none) member))
    (deftype method (union instance-method static-method))
    (deftype accessor (union instance-accessor static-accessor))
    
    (defrecord instance-variable
      (type class)
      (modifier (tag abstract virtual final))
      (immutable boolean))
    
    (defrecord variable
      (type (union (tag unknown) class))
      (value (union (tag unknown) object) :var)
      (immutable boolean))
    
    (defrecord instance-method
      (type signature)
      (code instance)  ;Method code
      (modifier (tag abstract virtual final)))
    
    (defrecord static-method
      (type signature)
      (code instance)  ;Method code
      (modifier (tag static constructor)))
    
    (defrecord instance-accessor
      (type class)
      (code instance)  ;Getter or setter function code
      (modifier (tag abstract virtual final)))
    
    (defrecord static-accessor
      (type class)
      (code instance)) ;Getter or setter function code
    
    
    (%heading (3 :semantics) "Method Closures")
    (deftuple method-closure
      (this object)
      (method method))
    
    
    (%heading (3 :semantics) "Prototype Instances")
    (defrecord prototype
      (parent prototype-opt)
      (dynamic-properties (list-set dynamic-property) :var))
    (deftype prototype-opt (union (tag none) prototype))
    
    (defrecord dynamic-property 
      (name string)
      (value object :var))
    
    
    (%heading (3 :semantics) "Class Instances")
    (deftype instance (union fixed-instance dynamic-instance))
    
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
      (id instance-variable)
      (value object :var))
    
    
    (%heading (2 :semantics) "Objects with Limits")
    (%text :comment (:label limited-instance instance) " must be an instance of " (:label limited-instance limit) " or one of "
           ( limited-instance limit) :apostrophe "s descendants.")
    (deftuple limited-instance
      (instance instance)
      (limit class))
    
    (deftype obj-optional-limit (union object limited-instance))
    
    
    (%heading (2 :semantics) "References")
    (deftuple variable-reference
      (env environment)
      (variable-multiname multiname))
    
    (deftuple dot-reference
      (base obj-optional-limit)
      (property-multiname multiname))
    
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
    
    (%text :comment "The first " (:type object) " is the " (:character-literal "this") " value. When the "
           (:type mode) " parameter is " (:tag compile) ", only compile-time expressions are allowed.")
    (deftype invoker (-> (object argument-list mode) object))
    
    
    (%heading (2 :semantics) "Unary Operators")
    (deftuple unary-method 
      (operand-type class)
      (f (-> (object object argument-list mode) object)))
    
    
    (%heading (2 :semantics) "Binary Operators")
    (deftuple binary-method
      (left-type class)
      (right-type class)
      (f (-> (object object mode) object)))
    
    
    (%heading (2 :semantics) "Modes of expression evaluation")
    (deftag compile)
    (deftag run)
    (deftype mode (tag compile run))
    
    
    (%heading (2 :semantics) "Contexts")
    (deftuple context
      (strict boolean)
      (wrap boolean)
      (inside-function boolean)
      (open-namespaces (list-set namespace))
      (break-labels (list-set label))
      (continue-labels (list-set label)))
    
    (define initial-context context
      (new context false false false (list-set public-namespace) (list-set-of label) (list-set-of label)))
    
    
    (%heading (3 :semantics) "Labels")
    (deftag default)
    (deftype label (union string (tag default)))
    
    
    (%heading (2 :semantics) "Environments")
    (%text :comment "An " (:type environment) " is a list of one or more frames. Each frame corresponds to a scope. "
           "More specific frames are listed first" :m-dash "each frame" :apostrophe "s scope is directly contained in the following frame"
           :apostrophe "s scope. The last frame is always the system frame.")
    (deftype environment (vector frame))
    (deftype frame (union regional-frame block-frame))
    (deftype regional-frame (union system-frame package-frame function-frame class))
    (deftype unreflected-frame (union system-frame package-frame function-frame block-frame))
    
    (defrecord system-frame
      (read-bindings (list-set binding) :var)
      (write-bindings (list-set binding) :var))
    
    (defrecord package-frame
      (read-bindings (list-set binding) :var)
      (write-bindings (list-set binding) :var)
      (internal-namespace namespace))
    
    (defrecord function-frame
      (read-bindings (list-set binding) :var)
      (write-bindings (list-set binding) :var)
      (this (union (tag none unknown) object)))
    
    (defrecord block-frame
      (read-bindings (list-set binding) :var)
      (write-bindings (list-set binding) :var))
    
    (define initial-environment environment (vector-of frame (new system-frame (list-set-of binding) (list-set-of binding))))
    
    
    (%heading (3 :semantics) "Environment Bindings")
    (deftype binding (union lexical-binding shadow-binding antibinding))
    
    (deftuple lexical-binding
      (qname qualified-name)
      (var variable)
      (hoisted boolean))
    
    
    (%text :comment "A " (:type shadow-binding) " present in a scope " (:variable "S")
           " prevents inheritance of a binding defined in an outer scope into " (:variable "S") ".")
    (deftuple shadow-binding
      (qname qualified-name))
    
    
    (%text :comment "An " (:type antibinding) " present in a scope " (:variable "S") " prevents the introduction of a binding into " (:variable "S") ".")
    (deftuple antibinding
      (qname qualified-name))
    
    
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
    (%text :comment (:global-call to-boolean o mode) " coerces an object " (:local o) " to a Boolean. If "
           (:local mode) " is " (:tag compile) ", only compile-time conversions are permitted.")
    (define (to-boolean (o object) (mode mode :unused)) boolean
      (case o
        (:select (union undefined null) (return false))
        (:narrow boolean (return o))
        (:narrow float64 (return (not-in o (tag +zero -zero nan))))
        (:narrow string (return (/= o "" string)))
        (:select (union namespace compound-attribute class method-closure prototype) (return true))
        (:select instance (todo))))
    
    (%heading (3 :semantics) (:global to-number nil))
    (%text :comment (:global-call to-number o mode) " coerces an object " (:local o) " to a number. If "
           (:local mode) " is " (:tag compile) ", only compile-time conversions are permitted.")
    (define (to-number (o object) (mode mode :unused)) float64
      (case o
        (:select undefined (return nan))
        (:select (union null (tag false)) (return +zero))
        (:select (tag true) (return 1.0))
        (:narrow float64 (return o))
        (:select string (todo))
        (:select (union namespace compound-attribute class method-closure) (throw type-error))
        (:select (union prototype instance) (todo))))
    
    (%heading (3 :semantics) (:global to-string nil))
    (%text :comment (:global-call to-string o mode) " coerces an object " (:local o) " to a string. If "
           (:local mode) " is " (:tag compile) ", only compile-time conversions are permitted.")
    (define (to-string (o object) (mode mode :unused)) string
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
    (define (to-primitive (o object) (hint object :unused) (mode mode)) object
      (case o
        (:select (union undefined null boolean float64 string) (return o))
        (:select (union namespace compound-attribute class method-closure prototype instance) (return (to-string o mode)))))
    
    
    (%heading (3 :semantics) (:global unary-plus nil))
    (%text :comment (:global-call unary-plus o mode) " returns the value of the unary expression " (:character-literal "+") (:local o) ". If "
           (:local mode) " is " (:tag compile) ", only compile-time operations are permitted.")
    (define (unary-plus (a obj-optional-limit) (mode mode)) object
      (return (unary-dispatch plus-table null a (new argument-list (vector-of object) (list-set-of named-argument)) mode)))
    
    (%heading (3 :semantics) (:global unary-not nil))
    (%text :comment (:global-call unary-not o mode) " returns the value of the unary expression " (:character-literal "!") (:local o) ". If "
           (:local mode) " is " (:tag compile) ", only compile-time operations are permitted.")
    (define (unary-not (a object) (mode mode)) object
      (return (not (to-boolean a mode))))
    
    
    (%heading (3 :semantics) "Attributes")
    (%text :comment (:global-call combine-attributes a b) " returns the attribute that results from concatenating the attributes "
           (:local a) " and " (:local b) ".")
    (define (combine-attributes (a attribute-opt-not-false) (b attribute)) attribute
      (cond
       ((in b false-type :narrow-false) (return false))
       ((in a (tag none true) :narrow-false) (return b))
       ((in b true-type :narrow-false) (return a))
       ((in a namespace :narrow-both)
        (cond
         ((= a b attribute) (return a))
         ((in b namespace :narrow-both)
          (return (new compound-attribute (list-set a b) none false false false none none false false)))
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
        (if (or (and (not-in (& extend a) (tag none)) (not-in (& extend b) (tag none)))
                (and (& enumerable a) (& enumerable b))
                (and (& dynamic a) (& dynamic b))
                (and (& compile a) (& compile b))
                (and (not-in (& member-mod a) (tag none)) (not-in (& member-mod b) (tag none)))
                (and (not-in (& override-mod a) (tag none)) (not-in (& override-mod b) (tag none)))
                (and (& prototype a) (& prototype b))
                (and (& unused a) (& unused b)))
          (throw type-error)
          (return (new compound-attribute
                       (set+ (& namespaces a) (& namespaces b))
                       (if (not-in (& extend a) (tag none)) (& extend a) (& extend b))
                       (or (& enumerable a) (& enumerable b))
                       (or (& dynamic a) (& dynamic b))
                       (or (& compile a) (& compile b))
                       (if (not-in (& member-mod a) (tag none)) (& member-mod a) (& member-mod b))
                       (if (not-in (& override-mod a) (tag none)) (& override-mod a) (& override-mod b))
                       (or (& prototype a) (& prototype b))
                       (or (& unused a) (& unused b))))))))
    
    
    (%text :comment (:global-call get-namespaces a) " returns the set of namespaces in attribute " (:local a) ".")
    (define (get-namespaces (a attribute-opt-not-false)) (list-set namespace)
      (case a
        (:select (tag none true) (return (list-set-of namespace)))
        (:narrow namespace (return (list-set a)))
        (:narrow compound-attribute (return (& namespaces a)))))
    
    
    (%heading (2 :semantics) "Objects with Limits")
    (%text :comment (:global-call get-object o) " returns " (:local o) " without its limit, if any.")
    (define (get-object (o obj-optional-limit)) object
      (case o
        (:narrow object (return o))
        (:narrow limited-instance (return (& instance o)))))
    
    (%text :comment (:global-call get-object-limit o) " returns " (:local o) :apostrophe "s limit or " (:tag none) " if none is provided.")
    (define (get-object-limit (o obj-optional-limit)) class-opt
      (case o
        (:narrow object (return none))
        (:narrow limited-instance (return (& limit o)))))
    
    
    (%heading (2 :semantics) "References")
    (%text :comment "If " (:local r) " is an " (:type object) ", " (:global-call read-reference r mode) " returns it unchanged.  If "
           (:local r) " is a " (:type reference) ", this function reads " (:local r) " and returns the result. If "
           (:local mode) " is " (:tag compile) ", only compile-time expressions can be evaluated in the process of reading " (:local r) ".")
    (define (read-reference (r obj-or-ref) (mode mode)) object
      (case r
        (:narrow object (return r))
        (:narrow variable-reference (return (read-variable (& env r) (& variable-multiname r) mode)))
        (:narrow dot-reference (return (read-property (& base r) (& property-multiname r) (list-set-of visibility-modifier none indexable enumerable) mode)))
        (:narrow bracket-reference (return (unary-dispatch bracket-read-table null (& base r) (& args r) mode)))))
    
    
    (%text :comment (:global-call read-ref-with-limit r mode) " reads the reference, if any, inside " (:local r)
           " and returns the result, retaining the same limit as " (:local r) ". If " (:local r)
           " has a limit " (:local limit) ", then the object read from the reference is checked to make sure that it is an instance of " (:local limit)
           " or one of its descendants. If "
           (:local mode) " is " (:tag compile) ", only compile-time expressions can be evaluated in the process of reading " (:local r) ".")
    (define (read-ref-with-limit (r obj-or-ref-optional-limit) (mode mode)) obj-optional-limit
      (case r
        (:narrow obj-or-ref (return (read-reference r mode)))
        (:narrow limited-obj-or-ref
          (const o object (read-reference (& ref r) mode))
          (const limit class (& limit r))
          (rwhen (= o null object)
            (return null))
          (rwhen (or (not-in o instance :narrow-false) (not (has-type o limit)))
            (throw type-error))
          (return (new limited-instance o limit)))))
    
    
    (%text :comment "If " (:local r) " is a reference, " (:global-call write-reference r o) " writes " (:local o) 
           " into " (:local r) ". An error occurs if " (:local r) " is not a reference. "
           (:local r) :apostrophe "s limit, if any, is ignored. "
           (:global write-reference) " is never called from a compile-time expression.")
    (define (write-reference (r obj-or-ref-optional-limit) (o object) (mode (tag run))) void
      (case r
        (:select object (throw reference-error))
        (:narrow variable-reference (write-variable (& env r) (& variable-multiname r) o mode))
        (:narrow dot-reference (write-property (& base r) (& property-multiname r) (list-set-of visibility-modifier none indexable enumerable) o mode))
        (:narrow bracket-reference
          (const args argument-list (new argument-list (cons o (& positional (& args r))) (& named (& args r))))
          (exec (unary-dispatch bracket-write-table null (& base r) args mode)))
        (:narrow limited-obj-or-ref (write-reference (& ref r) o mode))))
    
    
    (%text :comment "If " (:local r) " is a " (:type reference) ", " (:global-call delete-reference r) " deletes it. If "
           (:local r) " is an " (:type object) ", this function signals an error. "
           (:global delete-reference) " is never called from a compile-time expression.")
    (define (delete-reference (r obj-or-ref) (mode (tag run))) object
      (case r
        (:select object (throw reference-error))
        (:narrow variable-reference (return (delete-variable (& env r) (& variable-multiname r) mode)))
        (:narrow dot-reference (return (delete-property (& base r) (& property-multiname r) mode)))
        (:narrow bracket-reference (return (unary-dispatch bracket-delete-table null (& base r) (& args r) mode)))))
    
    
    (%text :comment (:global-call reference-base r) " returns " (:type reference) " " (:local r) :apostrophe "s base or"
           (:tag null) " if there is none. " (:local r) :apostrophe "s limit and the base" :apostrophe "s limit, if any, are ignored.")
    (define (reference-base (r obj-or-ref-optional-limit)) object
      (case r
        (:select (union object variable-reference) (return null))
        (:narrow (union dot-reference bracket-reference) (return (get-object (& base r))))
        (:narrow limited-obj-or-ref (return (reference-base (& ref r))))))
    
    
    (%heading (2 :semantics) "Slots")
    
    (define (find-slot (o object) (id instance-variable)) slot
      (assert (in o instance :narrow-true) (:local o) " must be an " (:type instance) ";")
      (const matching-slots (list-set slot)
        (map (& slots o) s s (= (& id s) id instance-variable)))
      (assert (= (length matching-slots) 1) "Note that exactly one slot should match: " (:assertion) ";")
      (return (elt-of matching-slots)))
    
    
    (%heading (2 :semantics) "Member Lookup")
    (%heading (3 :semantics) "Reading a Property")
    
    (define (read-property (ol obj-optional-limit) (multiname multiname) (visibilities (list-set visibility-modifier)) (mode mode)) object
      (const result (union (tag none none-yet) object) (read-object-property ol multiname visibilities mode))
      (case result
        (:narrow object (return result))
        (:select (tag none-yet) (return undefined))
        (:select (tag none) (throw property-not-found-error))))
    

    (define (read-object-property (ol obj-optional-limit) (multiname multiname) (visibilities (list-set visibility-modifier))
                                  (mode mode)) (union (tag none none-yet) object)
      (const qname qualified-name-opt (select-qualified-name (get-object ol) multiname read))
      (rwhen (in qname (tag none) :narrow-false)
        (return none))
      (var m member-opt)
      (case ol
        (:narrow (union undefined null boolean float64 string namespace compound-attribute method-closure fixed-instance)
          (<- m (most-specific-member (object-type ol) qname read instance visibilities)))
        (:narrow class
          (<- m (most-specific-member ol qname read static visibilities)))
        (:narrow prototype
          (rwhen (/= (& namespace qname) public-namespace namespace)
            (return none))
          (rwhen (in mode (tag compile))
            (throw compile-expression-error))
          (var p prototype-opt ol)
          (while (not-in p (tag none) :narrow-true)
            (const prop-value (union (tag none-yet) object) (read-dynamic-property p (& id qname)))
            (rwhen (not-in prop-value (tag none-yet) :narrow-true)
              (return prop-value))
            (<- p (& parent ol) :end-narrow))
          (return none-yet))
        (:narrow dynamic-instance
          (<- m (most-specific-member (object-type ol) qname read instance visibilities))
          (rwhen (and (in m (tag none)) (= (& namespace qname) public-namespace namespace))
            (rwhen (in mode (tag compile))
              (throw compile-expression-error))
            (return (read-dynamic-property ol (& id qname)))))
        (:narrow limited-instance
          (<- m (most-specific-member (& super (& limit ol)) qname read instance visibilities))))
      (const o object (get-object ol))
      (case m
        (:select (tag none) (return none))
        (:narrow instance-variable
          (rwhen (and (in mode (tag compile)) (not (& immutable m)))
            (throw compile-expression-error))
          (return (& value (find-slot o m))))
        (:narrow variable
          (rwhen (and (in mode (tag compile)) (not (& immutable m)))
            (throw compile-expression-error))
          (const value (union (tag unknown) object) (& value m))
          (rwhen (in value (tag unknown) :narrow-false)
            (throw uninitialised-error))
          (return value))
        (:narrow method (return (new method-closure o m)))
        (:narrow accessor (return ((& call (& code m)) o (new argument-list (vector-of object) (list-set-of named-argument)) mode)))))
    
    
    (define (read-dynamic-property (o (union prototype dynamic-instance)) (name string)) (union (tag none-yet) object)
      (reserve dp)
      (if (some (& dynamic-properties o) dp (= (& name dp) name string) :define-true)
        (return (& value dp))
        (return none-yet)))
    
    
    (%heading (3 :semantics) "Writing a Property")
    
    (define (write-property (ol obj-optional-limit) (multiname multiname) (visibilities (list-set visibility-modifier)) (new-value object) (mode (tag run))) void
      (const result (tag none ok) (write-object-property ol multiname visibilities new-value mode))
      (rwhen (in result (tag none))
        (throw property-not-found-error)))
    
    
    (define (write-object-property (ol obj-optional-limit) (multiname multiname) (visibilities (list-set visibility-modifier))
                                   (new-value object) (mode (tag run))) (tag none ok)
      (const qname qualified-name-opt (select-qualified-name (get-object ol) multiname write))
      (rwhen (in qname (tag none) :narrow-false)
        (return none))
      (var m member-opt)
      (case ol
        (:select (union undefined null boolean float64 string namespace compound-attribute method-closure)
          (return none))
        (:narrow class
          (<- m (most-specific-member ol qname write static visibilities)))
        (:narrow prototype
          (rwhen (/= (& namespace qname) public-namespace namespace)
            (return none))
          (write-dynamic-property ol (& id qname) new-value)
          (return ok))
        (:narrow fixed-instance
          (<- m (most-specific-member (object-type ol) qname write instance visibilities)))
        (:narrow dynamic-instance
          (<- m (most-specific-member (object-type ol) qname write instance visibilities))
          (rwhen (and (in m (tag none)) (= (& namespace qname) public-namespace namespace))
            (<- m (most-specific-member (object-type ol) qname read instance visibilities))
            (rwhen (not-in m (tag none))
              (return none))
            (write-dynamic-property ol (& id qname) new-value)
            (return ok)))
        (:narrow limited-instance
          (<- m (most-specific-member (& super (& limit ol)) qname write instance visibilities))))
      (const o object (get-object ol))
      (assert (not-in m method :narrow-true)
              (:local m) " cannot be a " (:type method) " at this point because all " (:type method)
              " properties are read-only;")
      (case m
        (:select (tag none) (return none))
        (:narrow instance-variable
          (assert (not (& immutable m))
                  (:local m) "." (:label instance-variable immutable) " must be " (:tag false)
                  " at this point because all immutable instance variables are read-only;")
          (rwhen (not (relaxed-has-type new-value (& type m)))
            (throw type-error))
          (&= value (find-slot o m) new-value)
          (return ok))
        (:narrow variable
          (assert (not (& immutable m))
                  (:local m) "." (:label variable immutable) " must be " (:tag false)
                  " at this point because all immutable static variables are read-only;")
          (const type (union (tag unknown) class) (& type m))
          (rwhen (or (in type (tag unknown) :narrow-false) (in (& value m) (tag unknown)))
            (throw uninitialised-error))
          (rwhen (not (relaxed-has-type new-value type))
            (throw type-error))
          (&= value m new-value)
          (return ok))
        (:narrow accessor
          (rwhen (not (relaxed-has-type new-value (& type m)))
            (throw type-error))
          (exec ((& call (& code m)) o (new argument-list (vector new-value) (list-set-of named-argument)) mode))
          (return ok))))
    
    
    (define (write-dynamic-property (o (union prototype dynamic-instance)) (name string) (new-value object)) void
      (reserve dp)
      (if (some (& dynamic-properties o) dp (= (& name dp) name string) :define-true)
        (&= value dp new-value)
        (&= dynamic-properties o (set+ (& dynamic-properties o) (list-set (new dynamic-property name new-value))))))
    
    
    (%heading (3 :semantics) "Deleting a Property")
    
    (define (delete-property (o obj-optional-limit :unused) (multiname multiname :unused) (mode (tag run) :unused)) boolean
      (todo))
    
    (define (delete-qualified-property (o object :unused) (name string :unused) (ns namespace :unused)
                                       (visibilities (list-set visibility-modifier) :unused) (mode (tag run) :unused)) boolean
      (todo))
    
    
    (%heading (3 :semantics) "Utilities")
    
    (deftag instance)
    (define (binding-has-category (b class-binding) (category (tag instance static))) boolean
      (case category
        (:select (tag instance) (return (in (& member b) instance-member)))
        (:select (tag static) (return (in (& member b) static-member)))))
    
    (deftag read)
    (deftag write)
    (deftag read-write)
    
    
    (define (most-specific-member (c class-opt) (qname qualified-name) (access (tag read write)) (category (tag instance static))
                                  (visibilities (list-set visibility-modifier))) member-opt
      (var c2 class-opt c)
      (while (not-in c2 (tag none) :narrow-true)
        (var bindings (list-set class-binding))
        (case access
          (:select (tag read) (<- bindings (& read-bindings c2)))
          (:select (tag write) (<- bindings (& write-bindings c2))))
        (reserve b)
        (rwhen (some bindings b (and (= (& qname b) qname qualified-name)
                                     (binding-has-category b category)
                                     (set-in (& visibility-modifier b) visibilities)) :define-true)
          (return (& member b)))
        (<- c2 (& super c2) :end-narrow))
      (return none))
    
    
    (define (select-qualified-name (o object) (multiname multiname) (access (tag read write))) qualified-name-opt
      (var qname qualified-name-opt)
      (if (in o class :narrow-true)
        (<- qname (select-qualified-name-in-class o multiname access static))
        (<- qname (select-qualified-name-in-class (object-type o) multiname access instance)))
      (rwhen (not-in qname (tag none) :narrow-true)
        (return qname))
      (reserve qname2)
      (rwhen (some multiname qname2 (= (& namespace qname2) public-namespace namespace) :define-true)
        (return qname2))
      (return none))
    
    
    ; ***** Note that this preferentially returns the public namespace when there is an ambiguity.
    (define (select-qualified-name-in-class (c class) (multiname multiname) (access (tag read write)) (category (tag instance static))) qualified-name-opt
      (const s class-opt (& super c))
      (when (not-in s (tag none) :narrow-true)
        (const qname qualified-name-opt (select-qualified-name-in-class s multiname access category))
        (rwhen (not-in qname (tag none) :narrow-true)
          (return qname)))
      (var bindings (list-set class-binding))
      (case access
        (:select (tag read) (<- bindings (& read-bindings c)))
        (:select (tag write) (<- bindings (& write-bindings c))))
      (const matching-bindings (list-set class-binding) (map bindings b b (and (set-in (& qname b) multiname) (binding-has-category b category))))
      (rwhen (nonempty matching-bindings)
        (const matching-members (list-set member) (map matching-bindings b (& member b)))
        (rwhen (> (length matching-members) 1)
          (// "This access is ambiguous because the bindings it found belong to several different members in the same class.")
          (throw property-not-found-error))
        ;(reserve b2) *****Not needed anymore?
        ;(rwhen (some matching-bindings b2 (= (& namespace (& qname b)) public-namespace namespace) :define-true)
        ;  (return (& qname b2)))
        (/* "Let " (:local b) ":" :nbsp (:type class-binding) " be any element of " (:local matching-bindings) ".")
        (const b class-binding (elt-of matching-bindings))
        (*/)
        (return (& qname b)))
      (return none))
    
    
    
    (%heading (2 :semantics) "Operator Dispatch")
    (%heading (3 :semantics) "Unary Operators")
    (%text :comment (:global-call unary-dispatch table this operand args mode) " dispatches the unary operator described by " (:local table)
           " applied to the " (:character-literal "this") " value " (:local this) ", the operand " (:local operand)
           ", and zero or more positional and/or named arguments " (:local args)
           ". If " (:local operand) " has a limit class, lookup is restricted to operators defined on the proper ancestors of that limit. If "
           (:local mode) " is " (:tag compile) ", only compile-time expressions can be evaluated in the process of dispatching and calling the operator.")
    (define (unary-dispatch (table (list-set unary-method)) (this object) (operand obj-optional-limit) (args argument-list) (mode mode)) object
      (const applicable-ops (list-set unary-method)
        (map table m m (limited-has-type operand (& operand-type m))))
      (reserve best)
      (rwhen (some applicable-ops best
                   (every applicable-ops m2 (is-ancestor (& operand-type m2) (& operand-type best))) :define-true)
        (return ((& f best) this (get-object operand) args mode)))
      (throw property-not-found-error))
    
    
    (%text :comment (:global-call limited-has-type o c) " returns " (:tag true) " if " (:local o) " is a member of class " (:local c)
           " with the added condition that, if " (:local o) " has a limit class " (:local limit) ", "
           (:local c) " is a proper ancestor of " (:local limit) ".")
    (define (limited-has-type (o obj-optional-limit) (c class)) boolean
      (const a object (get-object o))
      (const limit class-opt (get-object-limit o))
      (if (has-type a c)
        (if (in limit (tag none) :narrow-false)
          (return true)
          (return (is-proper-ancestor c limit)))
        (return false)))
    
    
    (%heading (3 :semantics) "Binary Operators")
    (%text :comment (:global-call is-binary-descendant m1 m2) " is " (:tag true) " if " (:local m1) " is at least as specific as " (:local m2)
           " as defined by the procedure below.")
    (define (is-binary-descendant (m1 binary-method) (m2 binary-method)) boolean
      (return (and (is-ancestor (& left-type m2) (& left-type m1))
                   (is-ancestor (& right-type m2) (& right-type m1)))))
    
    (%text :comment (:global-call binary-dispatch table left right mode) " dispatches the binary operator described by " (:local table)
           " applied to the operands " (:local left) " and " (:local right) ". If " (:local left) " has a limit " (:local left-limit)
           ", the lookup is restricted to operator definitions with an ancestor of " (:local left-limit)
           " for the left operand. Similarly, if " (:local right) " has a limit " (:local right-limit)
           ", the lookup is restricted to operator definitions with an ancestor of " (:local right-limit) " for the right operand. If "
           (:local mode) " is " (:tag compile) ", only compile-time expressions can be evaluated in the process of dispatching and calling the operator.")
    (define (binary-dispatch (table (list-set binary-method)) (left obj-optional-limit) (right obj-optional-limit) (mode mode)) object
      (const applicable-ops (list-set binary-method)
        (map table m m (and (limited-has-type left (& left-type m))
                            (limited-has-type right (& right-type m)))))
      (reserve best)
      (rwhen (some applicable-ops best
                   (every applicable-ops m2 (is-binary-descendant best m2)) :define-true)
        (return ((& f best) (get-object left) (get-object right) mode)))
      (throw property-not-found-error))
    
    
    (%heading (2 :semantics) "Contexts")
    
    (%text :comment (:global-call add-break-label cxt l) " returns a new " (:type context) " that is the same as "
           (:local cxt) " except that it includes the label " (:local l)
           " in the context" :apostrophe "s set of labels that are valid targets for a " (:character-literal "break") " statement.")
    (define (add-break-label (cxt context) (l label)) context
      (return (new context (& strict cxt) (& wrap cxt) (& inside-function cxt) (& open-namespaces cxt)
                   (set+ (& break-labels cxt) (list-set-of label l)) (& continue-labels cxt))))
    
    
    (%text :comment (:global-call add-continue-labels cxt ls) " returns a new " (:type context) " that is the same as "
           (:local cxt) " except that it includes the labels " (:local ls)
           " in the context" :apostrophe "s set of labels that are valid targets for a " (:character-literal "continue") " statement.")
    (define (add-continue-labels (cxt context) (ls (list-set label))) context
      (return (new context (& strict cxt) (& wrap cxt) (& inside-function cxt) (& open-namespaces cxt)
                   (& break-labels cxt) (set+ (& continue-labels cxt) ls))))
    
    
    (%heading (2 :semantics) "Environments")
    (%text :comment "If " (:local env) " is from within a class" :apostrophe "s body, "
           (:global-call get-enclosing-class env) " returns the innermost such class; otherwise, it returns " (:tag none) ".")
    (define (get-enclosing-class (env environment)) class-opt
      (reserve c)
      (rwhen (some env c (in c class :narrow-true) :define-true)
        (// "Let " (:local c) " be the first element of " (:local env) " that is a " (:type class) ".")
        (return c))
      (return none))
    
    
    (%text :comment (:global-call get-regional-frame env) "Returns the most specific frame in " (:local env)
           " that is not a local block frame.")
    (define (get-regional-frame (env environment)) regional-frame
      (const frame frame (nth env 0))
      (case frame
        (:narrow regional-frame (return frame))
        (:select block-frame (return (get-regional-frame (subseq env 1))))))
    
    
    (%text :comment (:global-call get-regional-environment env) " returns all frames in " (:local env) " up to and including the first regional frame.")
    (define (get-regional-environment (env environment)) (vector frame)
      (var i integer 0)
      (while (in (nth env i) block-frame)
        (<- i (+ i 1)))
      (return (subseq env 0 i)))
    
    
    (define (read-variable (env environment) (multiname multiname) (mode mode)) object
      (var i integer 0)
      (while (< i (length env))
        (const frame frame (nth env i))
        (var result (union (union (tag none none-yet) object)))
        (case frame
          (:narrow class (<- result (read-object-property frame multiname (list-set-of visibility-modifier none indexable enumerable) mode)))
          (:narrow unreflected-frame
            (<- result (read-frame-property frame multiname mode))))
        (rwhen (not-in result (tag none none-yet) :narrow-true)
          (return result))
        (<- i (+ i 1)))
      (throw reference-error))
    
    
    (define (read-frame-property (frame unreflected-frame) (multiname multiname) (mode mode)) (union (tag none) object)
      (const lexical-bindings (list-set lexical-binding)
        (map (& read-bindings frame) b b (and (in b lexical-binding :narrow-true) (set-in (& qname b) multiname))))
      (cond
       ((nonempty lexical-bindings)
        *****)
       (nil
        (const shadow-bindings (list-set shadow-binding)
          (map (& read-bindings frame) b b (and (in b shadow-binding :narrow-true) (set-in (& qname b) multiname))))
        (if (nonempty shadow-bindings)
          (throw reference-error)
          (return none)))))
    
    #|
    (define (find-frame-binding (frame unreflected-frame) (multiname multiname) (access (tag read write))) qualified-name-opt
      (const s class-opt (& super c))
      (when (not-in s (tag none) :narrow-true)
        (const qname qualified-name-opt (select-qualified-name-in-class s multiname access category))
        (rwhen (not-in qname (tag none) :narrow-true)
          (return qname)))
      (var bindings (list-set class-binding))
      (case access
        (:select (tag read) (<- bindings (& read-bindings c)))
        (:select (tag write) (<- bindings (& write-bindings c))))
      (const matching-bindings (list-set class-binding) (map bindings b b (and (set-in (& qname b) multiname) (binding-has-category b category))))
      (rwhen (nonempty matching-bindings)
        (const matching-members (list-set member) (map matching-bindings b (& member b)))
        (rwhen (> (length matching-members) 1)
          (// "This access is ambiguous because the bindings it found belong to several different members in the same class.")
          (throw property-not-found-error))
        ;(reserve b2) *****Not needed anymore?
        ;(rwhen (some matching-bindings b2 (= (& namespace (& qname b)) public-namespace namespace) :define-true)
        ;  (return (& qname b2)))
        (/* "Let " (:local b) ":" :nbsp (:type class-binding) " be any element of " (:local matching-bindings) ".")
        (const b class-binding (elt-of matching-bindings))
        (*/)
        (return (& qname b)))
      (return none))|#
    
    
    (define (write-variable (env environment :unused) (multiname multiname :unused) (new-value object :unused) (mode (tag run) :unused)) void
      (todo))
    
    (define (delete-variable (env environment :unused) (multiname multiname :unused) (mode (tag run) :unused)) boolean
      (todo))
    
    (%text :comment "Return the value of " (:character-literal "this") ". Throw an exception if there is no " (:character-literal "this") " defined.")
    (define (lookup-this (env environment :unused) (mode (tag run) :unused)) object
      (todo))
    
    
    (define (define-variable (cxt context) (env environment) (access (tag read read-write)) (hoisted boolean) (attributes attribute-opt-not-false)
              (name string)) variable
      (const regional-env (vector frame) (get-regional-environment env))
      (var namespaces (list-set namespace) (get-namespaces attributes))
      (when (empty namespaces)
        (<- namespaces (list-set public-namespace)))
      (// "***** If in a class, consider overrides and adjust list of namespaces.")
      (var excluded-namespaces (list-set namespace) namespaces)
      (when (nonempty (set* namespaces (& open-namespaces cxt)))
        (<- excluded-namespaces (set+ namespaces (& open-namespaces cxt))))
      (rwhen hoisted
        (assert (and (= access read-write (tag read read-write)) (= attributes none attribute-opt-not-false))
                "Note that only definitions with " (:assertion) " are hoisted.")
        (const regional-frame frame (nth regional-env (- (length regional-env) 1)))
        (assert (in regional-frame (union package-frame function-frame) :narrow-true)
                (:local env) " is either a " (:type package-frame) " or a " (:type function-frame)
                " because hoisting only occurs into package or function scope.")
        (check-for-antibindings regional-frame excluded-namespaces name)
        (todo))
      ;(check-definition-conflicts env access (new partial-name excluded-namespaces name))
      (todo))
    
    
    (define (check-for-antibindings (frame frame) (excluded-namespaces (list-set namespace)) (name string)) void
      (todo))
    
    #|
      ;(rwhen (some (& antibindings frame) b (and (= (& name 
    (define (define-hoisted-variable (env environment) (name string)) compile-binding
      (assert (in env (union package-compile-frame function-compile-frame block-compile-frame) :narrow-true)
              (:local env) " must be one of the frame types " (:type package-compile-frame) ", " (:type function-compile-frame) ", or "
              (:type block-compile-frame) " because hoisting only occurs into package or function scope.")
      (reserve b)
      (case env
        (:narrow block-compile-frame
          (rwhen (some (& bindings env) b (and (= (& name (& variable-multiname b)) name string) (set-in public-namespace (& namespaces (& variable-multiname b)))) :define-true)
            (// "A conflicting variable definition has been found.")
            (throw syntax-error))
          (return (define-hoisted-variable (& parent env) name)))
        (:narrow (union package-frame function-frame)
          (rwhen (some (& bindings env) b (and (= (& name (& variable-multiname b)) name string) (set-in public-namespace (& namespaces (& variable-multiname b)))) :define-true)
            (cond
             ((and (in b compile-binding :narrow-true) (& hoisted b))
              (return b))
             (nil
              (// "A conflicting variable definition has been found.")
              (throw syntax-error))))
          (const pn partial-name (new partial-name (list-set public-namespace) name))
          (const binding compile-binding (new compile-binding pn read-write unknown unknown true))
          (&= bindings env (set+ (& bindings env) (list-set-of binding binding)))
          (return binding))))|#
    
    
    #|(define (definition-partial-name (attributes attribute-opt-not-false) (name string)) partial-name
      (var namespaces (list-set namespace) (get-namespaces attributes))
      (when (empty namespaces)
        (<- namespaces (list-set public-namespace)))
      (return (new partial-name namespaces name)))|#
    
    
    ;(%text :comment (:global-call check-definition-conflicts env access multiname) " throws an error if there exists a definition in scope " (:local env)
    ;       " that would conflict with a definition of " (:local multiname) " with access " (:local access) ".")
    ;(define (check-definition-conflicts (env environment) (access (tag read read-write)) (multiname partial-name)) void
    ;  (todo))
    
    
    (define (create-variable (env environment :unused) (access (tag read read-write) :unused) (hoisted boolean :unused)
                             (attributes attribute-opt-not-false :unused) (name string :unused) (type class-opt :unused) (value (union (tag none) object) :unused)) void
      (todo))
    
    
    (%heading (3 :semantics) "Environment Bindings")
    
    (%text :comment (:global-call copy-bindings bindings) " returns a fresh copy of the given " (:local bindings) ".")
    (define (copy-bindings (bindings (list-set binding))) (list-set binding)
      (todo))
    
    
    
    
    (%heading 1 "Expressions")
    (grammar-argument :beta allow-in no-in)
    
    
    (%heading (2 :semantics) "Terminal Actions")
    
    (declare-action name $identifier string :action nil
      (terminal-action name $identifier identity))
    (declare-action value $number float64 :action nil
      (terminal-action value $number identity))
    (declare-action value $string string :action nil
      (terminal-action value $string identity))
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
    (rule :qualifier ((validate (-> (context environment) namespace)))
      (production :qualifier (:identifier) qualifier-identifier
        ((validate cxt env)
         (const name (list-set qualified-name) (map (& open-namespaces cxt) ns (new qualified-name ns (name :identifier))))
         (const a object (read-variable env name compile))
         (rwhen (not-in a namespace :narrow-false) (throw type-error))
         (return a)))
      (production :qualifier (public) qualifier-public
        ((validate (cxt :unused) (env :unused))
         (return public-namespace)))
      (production :qualifier (private) qualifier-private
        ((validate (cxt :unused) env)
         (const c class-opt (get-enclosing-class env))
         (rwhen (in c (tag none) :narrow-false)
           (throw syntax-error))
         (return (& private-namespace c)))))
    
    (rule :simple-qualified-identifier ((multiname (writable-cell multiname)) (validate (-> (context environment) void)))
      (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier
        ((validate cxt (env :unused))
         (const multiname (list-set qualified-name) (map (& open-namespaces cxt) ns (new qualified-name ns (name :identifier))))
         (action<- (multiname :simple-qualified-identifier 0) multiname)))
      (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier
        ((validate cxt env)
         (const q namespace ((validate :qualifier) cxt env))
         (action<- (multiname :simple-qualified-identifier 0) (list-set (new qualified-name q (name :identifier)))))))
    
    (rule :expression-qualified-identifier ((multiname (writable-cell multiname)) (validate (-> (context environment) void)))
      (production :expression-qualified-identifier (:paren-expression \:\: :identifier) expression-qualified-identifier-identifier
        ((validate cxt env)
         ((validate :paren-expression) cxt env)
         (const r obj-or-ref ((eval :paren-expression) env compile))
         (const q object (read-reference r compile))
         (rwhen (not-in q namespace :narrow-false) (throw type-error))
         (action<- (multiname :expression-qualified-identifier 0) (list-set (new qualified-name q (name :identifier)))))))
    
    (rule :qualified-identifier ((multiname (writable-cell multiname)) (validate (-> (context environment) void)))
      (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple
        ((validate cxt env)
         ((validate :simple-qualified-identifier) cxt env)
         (action<- (multiname :qualified-identifier 0) (multiname :simple-qualified-identifier))))
      (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression
        ((validate cxt env)
         ((validate :expression-qualified-identifier) cxt env)
         (action<- (multiname :qualified-identifier 0) (multiname :expression-qualified-identifier)))))
    (%print-actions ("Validation and Evaluation" multiname validate))
    
    
    (%heading 2 "Unit Expressions")
    (rule :unit-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :unit-expression (:paren-list-expression) unit-expression-paren-list-expression
        ((validate cxt env) ((validate :paren-list-expression) cxt env))
        ((eval env mode) (return ((eval :paren-list-expression) env mode))))
      (production :unit-expression ($number :no-line-break $string) unit-expression-number-with-unit
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval (env :unused) (mode :unused)) (todo)))
      (production :unit-expression (:unit-expression :no-line-break $string) unit-expression-unit-expression-with-unit
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval (env :unused) (mode :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (%heading 2 "Primary Expressions")
    (rule :primary-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :primary-expression (null) primary-expression-null
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return null)))
      (production :primary-expression (true) primary-expression-true
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return true)))
      (production :primary-expression (false) primary-expression-false
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return false)))
      (production :primary-expression (public) primary-expression-public
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return public-namespace)))
      (production :primary-expression ($number) primary-expression-number
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return (value $number))))
      (production :primary-expression ($string) primary-expression-string
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return (value $string))))
      (production :primary-expression (this) primary-expression-this
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (return (lookup-this env mode))))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (todo)))
      (production :primary-expression (:unit-expression) primary-expression-unit-expression
        ((validate cxt env) ((validate :unit-expression) cxt env))
        ((eval env mode) (return ((eval :unit-expression) env mode))))
      (production :primary-expression (:array-literal) primary-expression-array-literal
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval (env :unused) (mode :unused)) (todo)))
      (production :primary-expression (:object-literal) primary-expression-object-literal
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval (env :unused) (mode :unused)) (todo)))
      (production :primary-expression (:function-expression) primary-expression-function-expression
        ((validate cxt env) ((validate :function-expression) cxt env))
        ((eval env mode) (return ((eval :function-expression) env mode)))))
    
    (rule :paren-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :paren-expression (\( (:assignment-expression allow-in) \)) paren-expression-assignment-expression
        (validate (validate :assignment-expression))
        (eval (eval :assignment-expression))))
    
    (rule :paren-list-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref))
                                  (eval-as-list (-> (environment mode) (vector object))))
      (production :paren-list-expression (:paren-expression) paren-list-expression-paren-expression
        ((validate cxt env) ((validate :paren-expression) cxt env))
        ((eval env mode) (return ((eval :paren-expression) env mode)))
        ((eval-as-list env mode)
         (const r obj-or-ref ((eval :paren-expression) env mode))
         (const elt object (read-reference r mode))
         (return (vector elt))))
      (production :paren-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) paren-list-expression-list-expression
        ((validate cxt env)
         ((validate :list-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env mode)
         (const ra obj-or-ref ((eval :list-expression) env mode))
         (exec (read-reference ra mode))
         (const rb obj-or-ref ((eval :assignment-expression) env mode))
         (return (read-reference rb mode)))
        ((eval-as-list env mode)
         (const elts (vector object) ((eval-as-list :list-expression) env mode))
         (const r obj-or-ref ((eval :assignment-expression) env mode))
         (const elt object (read-reference r mode))
         (return (append elts (vector elt))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Function Expressions")
    (rule :function-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :function-expression (function :function-signature :block) function-expression-anonymous
        ((validate (cxt :unused) (env :unused)) (todo)) ;***** Clear break and continue inside cxt
        ((eval (env :unused) (mode :unused)) (todo)))
      (production :function-expression (function :identifier :function-signature :block) function-expression-named
        ((validate (cxt :unused) (env :unused)) (todo)) ;***** Clear break and continue inside cxt
        ((eval (env :unused) (mode :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Object Literals")
    (production :object-literal (\{ \}) object-literal-empty)
    (production :object-literal (\{ :field-list \}) object-literal-list)
    
    (production :field-list (:literal-field) field-list-one)
    (production :field-list (:field-list \, :literal-field) field-list-more)
    
    (rule :literal-field ((validate (-> (context environment) (list-set string))) (eval (-> (environment mode) named-argument)))
      (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression
        ((validate cxt env)
         (const names (list-set string) ((validate :field-name) cxt env))
         ((validate :assignment-expression) cxt env)
         (return names))
        ((eval env mode)
         (const name string ((eval :field-name) env mode))
         (const r obj-or-ref ((eval :assignment-expression) env mode))
         (const value object (read-reference r mode))
         (return (new named-argument name value)))))
    
    (rule :field-name ((validate (-> (context environment) (list-set string))) (eval (-> (environment mode) string)))
      (production :field-name (:identifier) field-name-identifier
        ((validate (cxt :unused) (env :unused)) (return (list-set (name :identifier))))
        ((eval (env :unused) (mode :unused)) (return (name :identifier))))
      (production :field-name ($string) field-name-string
        ((validate (cxt :unused) (env :unused)) (return (list-set (value $string))))
        ((eval (env :unused) (mode :unused)) (return (value $string))))
      (production :field-name ($number) field-name-number
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval (env :unused) (mode :unused)) (todo)))
      (? js2
        (production :field-name (:paren-expression) field-name-paren-expression
          ((validate (cxt :unused) (env :unused)) (todo))
          ((eval (env :unused) (mode :unused)) (todo)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Super Expressions")
    (rule :super-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production :super-expression (super) super-expression-super
        ((validate (cxt :unused) env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw syntax-error))
         (todo))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const this object (lookup-this env mode))
         (const limit class-opt (get-enclosing-class env))
         (assert (not-in limit (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local limit) " cannot be " (:tag none) " at this point.")
         (return (new limited-obj-or-ref this limit))))
      (production :super-expression (:full-super-expression) super-expression-full-super-expression
        ((validate cxt env) ((validate :full-super-expression) cxt env))
        ((eval env mode) (return ((eval :full-super-expression) env mode)))))
    
    (rule :full-super-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production :full-super-expression (super :paren-expression) full-super-expression-super-paren-expression
        ((validate cxt env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw syntax-error))
         ((validate :paren-expression) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :paren-expression) env mode))
         (const limit class-opt (get-enclosing-class env))
         (assert (not-in limit (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local limit) " cannot be " (:tag none) " at this point.")
         (return (new limited-obj-or-ref r limit)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Postfix Expressions")
    (rule :postfix-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        (validate (validate :attribute-expression))
        (eval (eval :attribute-expression)))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        (validate (validate :full-postfix-expression))
        (eval (eval :full-postfix-expression)))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        (validate (validate :short-new-expression))
        (eval (eval :short-new-expression))))
    
    (rule :postfix-expression-or-super ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production :postfix-expression-or-super (:postfix-expression) postfix-expression-or-super-postfix-expression
        (validate (validate :postfix-expression))
        (eval (eval :postfix-expression)))
      (production :postfix-expression-or-super (:super-expression) postfix-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    
    (rule :attribute-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier
        ((validate cxt env) ((validate :simple-qualified-identifier) cxt env))
        ((eval env (mode :unused)) (return (new variable-reference env (multiname :simple-qualified-identifier)))))
      (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator
        ((validate cxt env)
         ((validate :attribute-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :attribute-expression) env mode))
         (const a object (read-reference r mode))
         (return ((eval :member-operator) env a mode))))
      (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call
        ((validate cxt env)
         ((validate :attribute-expression) cxt env)
         ((validate :arguments) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :attribute-expression) env mode))
         (const f object (read-reference r mode))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env mode))
         (return (unary-dispatch call-table base f args mode)))))
    
    (rule :full-postfix-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression
        ((validate cxt env) ((validate :primary-expression) cxt env))
        ((eval env mode) (return ((eval :primary-expression) env mode))))
      (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier
        ((validate cxt env) ((validate :expression-qualified-identifier) cxt env))
        ((eval env (mode :unused)) (return (new variable-reference env (multiname :expression-qualified-identifier)))))
      (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression
        ((validate cxt env) ((validate :full-new-expression) cxt env))
        ((eval env mode) (return ((eval :full-new-expression) env mode))))
      (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator
        ((validate cxt env)
         ((validate :full-postfix-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :full-postfix-expression) env mode))
         (const a object (read-reference r mode))
         (return ((eval :member-operator) env a mode))))
      (production :full-postfix-expression (:super-expression :dot-operator) full-postfix-expression-super-dot-operator
        ((validate cxt env)
         ((validate :super-expression) cxt env)
         ((validate :dot-operator) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :super-expression) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (return ((eval :dot-operator) env a mode))))
      (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call
        ((validate cxt env)
         ((validate :full-postfix-expression) cxt env)
         ((validate :arguments) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :full-postfix-expression) env mode))
         (const f object (read-reference r mode))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env mode))
         (return (unary-dispatch call-table base f args mode))))
      (production :full-postfix-expression (:full-super-expression :arguments) full-postfix-expression-super-call
        ((validate cxt env)
         ((validate :full-super-expression) cxt env)
         ((validate :arguments) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :full-super-expression) env mode))
         (const f obj-optional-limit (read-ref-with-limit r mode))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env mode))
         (return (unary-dispatch call-table base f args mode))))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break ++) full-postfix-expression-increment
        ((validate cxt env) ((validate :postfix-expression-or-super) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (const b object (unary-dispatch increment-table null a (new argument-list (vector-of object) (list-set-of named-argument)) mode))
         (write-reference r b mode)
         (return (get-object a))))
      (production :full-postfix-expression (:postfix-expression-or-super :no-line-break --) full-postfix-expression-decrement
        ((validate cxt env) ((validate :postfix-expression-or-super) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (const b object (unary-dispatch decrement-table null a (new argument-list (vector-of object) (list-set-of named-argument)) mode))
         (write-reference r b mode)
         (return (get-object a)))))
    
    (rule :full-new-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new
        ((validate cxt env)
         ((validate :full-new-subexpression) cxt env)
         ((validate :arguments) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :full-new-subexpression) env mode))
         (const f object (read-reference r mode))
         (const args argument-list ((eval :arguments) env mode))
         (return (unary-dispatch construct-table null f args mode))))
      (production :full-new-expression (new :full-super-expression :arguments) full-new-expression-super-new
        ((validate cxt env)
         ((validate :full-super-expression) cxt env)
         ((validate :arguments) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :full-super-expression) env mode))
         (const f obj-optional-limit (read-ref-with-limit r mode))
         (const args argument-list ((eval :arguments) env mode))
         (return (unary-dispatch construct-table null f args mode)))))
    
    (rule :full-new-subexpression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression
        ((validate cxt env) ((validate :primary-expression) cxt env))
        ((eval env mode) (return ((eval :primary-expression) env mode))))
      (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier
        ((validate cxt env) ((validate :qualified-identifier) cxt env))
        ((eval env (mode :unused)) (return (new variable-reference env (multiname :qualified-identifier)))))
      (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression
        ((validate cxt env) ((validate :full-new-expression) cxt env))
        ((eval env mode) (return ((eval :full-new-expression) env mode))))
      (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator
        ((validate cxt env)
         ((validate :full-new-subexpression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :full-new-subexpression) env mode))
         (const a object (read-reference r mode))
         (return ((eval :member-operator) env a mode))))
      (production :full-new-subexpression (:super-expression :dot-operator) full-new-subexpression-super-dot-operator
        ((validate cxt env)
         ((validate :super-expression) cxt env)
         ((validate :dot-operator) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :super-expression) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (return ((eval :dot-operator) env a mode)))))
    
    (rule :short-new-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :short-new-expression (new :short-new-subexpression) short-new-expression-new
        ((validate cxt env) ((validate :short-new-subexpression) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :short-new-subexpression) env mode))
         (const f object (read-reference r mode))
         (return (unary-dispatch construct-table null f (new argument-list (vector-of object) (list-set-of named-argument)) mode))))
      (production :short-new-expression (new :super-expression) short-new-expression-super-new
        ((validate cxt env) ((validate :super-expression) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :super-expression) env mode))
         (const f obj-optional-limit (read-ref-with-limit r mode))
         (return (unary-dispatch construct-table null f (new argument-list (vector-of object) (list-set-of named-argument)) mode)))))
    
    (rule :short-new-subexpression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full
        (validate (validate :full-new-subexpression))
        (eval (eval :full-new-subexpression)))
      (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short
        (validate (validate :short-new-expression))
        (eval (eval :short-new-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Member Operators")
    (rule :member-operator ((validate (-> (context environment) void)) (eval (-> (environment object mode) obj-or-ref)))
      (production :member-operator (:dot-operator) member-operator-dot-operator
        ((validate cxt env) ((validate :dot-operator) cxt env))
        ((eval env base mode) (return ((eval :dot-operator) env base mode))))
      (production :member-operator (\. :paren-expression) member-operator-indirect
        ((validate cxt env) ((validate :paren-expression) cxt env))
        ((eval (env :unused) (base :unused) (mode :unused)) (todo))))
    
    (rule :dot-operator ((validate (-> (context environment) void)) (eval (-> (environment obj-optional-limit mode) obj-or-ref)))
      (production :dot-operator (\. :qualified-identifier) dot-operator-qualified-identifier
        ((validate cxt env) ((validate :qualified-identifier) cxt env))
        ((eval (env :unused) base (mode :unused)) (return (new dot-reference base (multiname :qualified-identifier)))))
      (production :dot-operator (:brackets) dot-operator-brackets
        ((validate cxt env) ((validate :brackets) cxt env))
        ((eval env base mode)
         (const args argument-list ((eval :brackets) env mode))
         (return (new bracket-reference base args)))))
    
    (rule :brackets ((validate (-> (context environment) void)) (eval (-> (environment mode) argument-list)))
      (production :brackets ([ ]) brackets-none
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed
        ((validate cxt env) ((validate :list-expression) cxt env))
        ((eval env mode)
         (const positional (vector object) ((eval-as-list :list-expression) env mode))
         (return (new argument-list positional (list-set-of named-argument)))))
      (production :brackets ([ :named-argument-list ]) brackets-named
        ((validate cxt env) (exec ((validate :named-argument-list) cxt env)))
        ((eval env mode) (return ((eval :named-argument-list) env mode)))))
    
    (rule :arguments ((validate (-> (context environment) void)) (eval (-> (environment mode) argument-list)))
      (production :arguments (:paren-expressions) arguments-paren-expressions
        ((validate cxt env) ((validate :paren-expressions) cxt env))
        ((eval env mode) (return ((eval :paren-expressions) env mode))))
      (production :arguments (\( :named-argument-list \)) arguments-named
        ((validate cxt env) (exec ((validate :named-argument-list) cxt env)))
        ((eval env mode) (return ((eval :named-argument-list) env mode)))))
    
    (rule :paren-expressions ((validate (-> (context environment) void)) (eval (-> (environment mode) argument-list)))
      (production :paren-expressions (\( \)) paren-expressions-none
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :paren-expressions (:paren-list-expression) paren-expressions-some
        ((validate cxt env) ((validate :paren-list-expression) cxt env))
        ((eval env mode)
         (const positional (vector object) ((eval-as-list :paren-list-expression) env mode))
         (return (new argument-list positional (list-set-of named-argument))))))
    
    (rule :named-argument-list ((validate (-> (context environment) (list-set string))) (eval (-> (environment mode) argument-list)))
      (production :named-argument-list (:literal-field) named-argument-list-one
        ((validate cxt env) (return ((validate :literal-field) cxt env)))
        ((eval env mode)
         (const na named-argument ((eval :literal-field) env mode))
         (return (new argument-list (vector-of object) (list-set na)))))
      (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed
        ((validate cxt env)
         ((validate :list-expression) cxt env)
         (return ((validate :literal-field) cxt env)))
        ((eval env mode)
         (const positional (vector object) ((eval-as-list :list-expression) env mode))
         (const na named-argument ((eval :literal-field) env mode))
         (return (new argument-list positional (list-set na)))))
      (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more
        ((validate cxt env)
         (const names1 (list-set string) ((validate :named-argument-list) cxt env))
         (const names2 (list-set string) ((validate :literal-field) cxt env))
         (rwhen (nonempty (set* names1 names2))
           (throw syntax-error))
         (return (set+ names1 names2)))
        ((eval env mode)
         (const args argument-list ((eval :named-argument-list) env mode))
         (const na named-argument ((eval :literal-field) env mode))
         (rwhen (some (& named args) na2 (= (& name na2) (& name na) string))
           (throw argument-mismatch-error))
         (return (new argument-list (& positional args) (set+ (& named args) (list-set na)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Unary Operators")
    (rule :unary-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :unary-expression (:postfix-expression) unary-expression-postfix
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((eval env mode) (return ((eval :postfix-expression) env mode))))
      (production :unary-expression (delete :postfix-expression) unary-expression-delete
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env mode))
         (return (delete-reference r mode))))
      (production :unary-expression (void :unary-expression) unary-expression-void
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :unary-expression) env mode))
         (exec (read-reference r mode))
         (return undefined)))
      (production :unary-expression (typeof :unary-expression) unary-expression-typeof
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :unary-expression) env mode))
         (const a object (read-reference r mode))
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
        ((validate cxt env) ((validate :postfix-expression-or-super) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (const b object (unary-dispatch increment-table null a (new argument-list (vector-of object) (list-set-of named-argument)) mode))
         (write-reference r b mode)
         (return b)))
      (production :unary-expression (-- :postfix-expression-or-super) unary-expression-decrement
        ((validate cxt env) ((validate :postfix-expression-or-super) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref-optional-limit ((eval :postfix-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (const b object (unary-dispatch decrement-table null a (new argument-list (vector-of object) (list-set-of named-argument)) mode))
         (write-reference r b mode)
         (return b)))
      (production :unary-expression (+ :unary-expression-or-super) unary-expression-plus
        ((validate cxt env) ((validate :unary-expression-or-super) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :unary-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (return (unary-plus a mode))))
      (production :unary-expression (- :unary-expression-or-super) unary-expression-minus
        ((validate cxt env) ((validate :unary-expression-or-super) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :unary-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (return (unary-dispatch minus-table null a (new argument-list (vector-of object) (list-set-of named-argument)) mode))))
      (production :unary-expression (~ :unary-expression-or-super) unary-expression-bitwise-not
        ((validate cxt env) ((validate :unary-expression-or-super) cxt env))
        ((eval env mode)
         (const r obj-or-ref-optional-limit ((eval :unary-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit r mode))
         (return (unary-dispatch bitwise-not-table null a (new argument-list (vector-of object) (list-set-of named-argument)) mode))))
      (production :unary-expression (! :unary-expression) unary-expression-logical-not
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :unary-expression) env mode))
         (const a object (read-reference r mode))
         (return (unary-not a mode)))))
    
    (rule :unary-expression-or-super ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production :unary-expression-or-super (:unary-expression) unary-expression-or-super-unary-expression
        (validate (validate :unary-expression))
        (eval (eval :unary-expression)))
      (production :unary-expression-or-super (:super-expression) unary-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Multiplicative Operators")
    (rule :multiplicative-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env mode) (return ((eval :unary-expression) env mode))))
      (production :multiplicative-expression (:multiplicative-expression-or-super * :unary-expression-or-super) multiplicative-expression-multiply
        ((validate cxt env)
         ((validate :multiplicative-expression-or-super) cxt env)
         ((validate :unary-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :multiplicative-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :unary-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch multiply-table a b mode))))
      (production :multiplicative-expression (:multiplicative-expression-or-super / :unary-expression-or-super) multiplicative-expression-divide
        ((validate cxt env)
         ((validate :multiplicative-expression-or-super) cxt env)
         ((validate :unary-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :multiplicative-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :unary-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch divide-table a b mode))))
      (production :multiplicative-expression (:multiplicative-expression-or-super % :unary-expression-or-super) multiplicative-expression-remainder
        ((validate cxt env)
         ((validate :multiplicative-expression-or-super) cxt env)
         ((validate :unary-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :multiplicative-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :unary-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch remainder-table a b mode)))))
    
    (rule :multiplicative-expression-or-super ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production :multiplicative-expression-or-super (:multiplicative-expression) multiplicative-expression-or-super-multiplicative-expression
        (validate (validate :multiplicative-expression))
        (eval (eval :multiplicative-expression)))
      (production :multiplicative-expression-or-super (:super-expression) multiplicative-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Additive Operators")
    (rule :additive-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        ((validate cxt env) ((validate :multiplicative-expression) cxt env))
        ((eval env mode) (return ((eval :multiplicative-expression) env mode))))
      (production :additive-expression (:additive-expression-or-super + :multiplicative-expression-or-super) additive-expression-add
        ((validate cxt env)
         ((validate :additive-expression-or-super) cxt env)
         ((validate :multiplicative-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :additive-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :multiplicative-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch add-table a b mode))))
      (production :additive-expression (:additive-expression-or-super - :multiplicative-expression-or-super) additive-expression-subtract
        ((validate cxt env)
         ((validate :additive-expression-or-super) cxt env)
         ((validate :multiplicative-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :additive-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :multiplicative-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch subtract-table a b mode)))))
    
    (rule :additive-expression-or-super ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production :additive-expression-or-super (:additive-expression) additive-expression-or-super-additive-expression
        (validate (validate :additive-expression))
        (eval (eval :additive-expression)))
      (production :additive-expression-or-super (:super-expression) additive-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Bitwise Shift Operators")
    (rule :shift-expression ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        ((validate cxt env) ((validate :additive-expression) cxt env))
        ((eval env mode) (return ((eval :additive-expression) env mode))))
      (production :shift-expression (:shift-expression-or-super << :additive-expression-or-super) shift-expression-left
        ((validate cxt env)
         ((validate :shift-expression-or-super) cxt env)
         ((validate :additive-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :shift-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :additive-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch shift-left-table a b mode))))
      (production :shift-expression (:shift-expression-or-super >> :additive-expression-or-super) shift-expression-right-signed
        ((validate cxt env)
         ((validate :shift-expression-or-super) cxt env)
         ((validate :additive-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :shift-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :additive-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch shift-right-table a b mode))))
      (production :shift-expression (:shift-expression-or-super >>> :additive-expression-or-super) shift-expression-right-unsigned
        ((validate cxt env)
         ((validate :shift-expression-or-super) cxt env)
         ((validate :additive-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :shift-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :additive-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch shift-right-unsigned-table a b mode)))))
    
    (rule :shift-expression-or-super ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production :shift-expression-or-super (:shift-expression) shift-expression-or-super-shift-expression
        (validate (validate :shift-expression))
        (eval (eval :shift-expression)))
      (production :shift-expression-or-super (:super-expression) shift-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Relational Operators")
    (rule (:relational-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
        ((validate cxt env) ((validate :shift-expression) cxt env))
        ((eval env mode) (return ((eval :shift-expression) env mode))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) < :shift-expression-or-super) relational-expression-less
        ((validate cxt env)
         ((validate :relational-expression-or-super) cxt env)
         ((validate :shift-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :shift-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch less-table a b mode))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) > :shift-expression-or-super) relational-expression-greater
        ((validate cxt env)
         ((validate :relational-expression-or-super) cxt env)
         ((validate :shift-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :shift-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch less-table b a mode))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) <= :shift-expression-or-super) relational-expression-less-or-equal
        ((validate cxt env)
         ((validate :relational-expression-or-super) cxt env)
         ((validate :shift-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :shift-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch less-or-equal-table a b mode))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) >= :shift-expression-or-super) relational-expression-greater-or-equal
        ((validate cxt env)
         ((validate :relational-expression-or-super) cxt env)
         ((validate :shift-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :shift-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch less-or-equal-table b a mode))))
      (production (:relational-expression :beta) ((:relational-expression :beta) is :shift-expression) relational-expression-is
        ((validate cxt env)
         ((validate :relational-expression) cxt env)
         ((validate :shift-expression) cxt env))
        ((eval (env :unused) (mode :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((validate cxt env)
         ((validate :relational-expression) cxt env)
         ((validate :shift-expression) cxt env))
        ((eval (env :unused) (mode :unused)) (todo)))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression-or-super) relational-expression-in
        ((validate cxt env)
         ((validate :relational-expression) cxt env)
         ((validate :shift-expression-or-super) cxt env))
        ((eval (env :unused) (mode :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((validate cxt env)
         ((validate :relational-expression) cxt env)
         ((validate :shift-expression) cxt env))
        ((eval (env :unused) (mode :unused)) (todo))))
    
    (rule (:relational-expression-or-super :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production (:relational-expression-or-super :beta) ((:relational-expression :beta)) relational-expression-or-super-relational-expression
        (validate (validate :relational-expression))
        (eval (eval :relational-expression)))
      (production (:relational-expression-or-super :beta) (:super-expression) relational-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Equality Operators")
    (rule (:equality-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        ((validate cxt env) ((validate :relational-expression) cxt env))
        ((eval env mode) (return ((eval :relational-expression) env mode))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) == (:relational-expression-or-super :beta)) equality-expression-equal
        ((validate cxt env)
         ((validate :equality-expression-or-super) cxt env)
         ((validate :relational-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :equality-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch equal-table a b mode))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) != (:relational-expression-or-super :beta)) equality-expression-not-equal
        ((validate cxt env)
         ((validate :equality-expression-or-super) cxt env)
         ((validate :relational-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :equality-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (const c object (binary-dispatch equal-table a b mode))
         (return (unary-not c mode))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) === (:relational-expression-or-super :beta)) equality-expression-strict-equal
        ((validate cxt env)
         ((validate :equality-expression-or-super) cxt env)
         ((validate :relational-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :equality-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch strict-equal-table a b mode))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) !== (:relational-expression-or-super :beta)) equality-expression-strict-not-equal
        ((validate cxt env)
         ((validate :equality-expression-or-super) cxt env)
         ((validate :relational-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :equality-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :relational-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (const c object (binary-dispatch strict-equal-table a b mode))
         (return (unary-not c mode)))))
    
    (rule (:equality-expression-or-super :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production (:equality-expression-or-super :beta) ((:equality-expression :beta)) equality-expression-or-super-equality-expression
        (validate (validate :equality-expression))
        (eval (eval :equality-expression)))
      (production (:equality-expression-or-super :beta) (:super-expression) equality-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        ((validate cxt env) ((validate :equality-expression) cxt env))
        ((eval env mode) (return ((eval :equality-expression) env mode))))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression-or-super :beta) & (:equality-expression-or-super :beta)) bitwise-and-expression-and
        ((validate cxt env)
         ((validate :bitwise-and-expression-or-super) cxt env)
         ((validate :equality-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :bitwise-and-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :equality-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch bitwise-and-table a b mode)))))
    
    (rule (:bitwise-xor-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        ((validate cxt env) ((validate :bitwise-and-expression) cxt env))
        ((eval env mode) (return ((eval :bitwise-and-expression) env mode))))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression-or-super :beta) ^ (:bitwise-and-expression-or-super :beta)) bitwise-xor-expression-xor
        ((validate cxt env)
         ((validate :bitwise-xor-expression-or-super) cxt env)
         ((validate :bitwise-and-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :bitwise-xor-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :bitwise-and-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch bitwise-xor-table a b mode)))))
    
    (rule (:bitwise-or-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        ((validate cxt env) ((validate :bitwise-xor-expression) cxt env))
        ((eval env mode) (return ((eval :bitwise-xor-expression) env mode))))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression-or-super :beta) \| (:bitwise-xor-expression-or-super :beta)) bitwise-or-expression-or
        ((validate cxt env)
         ((validate :bitwise-or-expression-or-super) cxt env)
         ((validate :bitwise-xor-expression-or-super) cxt env))
        ((eval env mode)
         (const ra obj-or-ref-optional-limit ((eval :bitwise-or-expression-or-super) env mode))
         (const a obj-optional-limit (read-ref-with-limit ra mode))
         (const rb obj-or-ref-optional-limit ((eval :bitwise-xor-expression-or-super) env mode))
         (const b obj-optional-limit (read-ref-with-limit rb mode))
         (return (binary-dispatch bitwise-or-table a b mode)))))
    
    
    (rule (:bitwise-and-expression-or-super :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production (:bitwise-and-expression-or-super :beta) ((:bitwise-and-expression :beta)) bitwise-and-expression-or-super-bitwise-and-expression
        (validate (validate :bitwise-and-expression))
        (eval (eval :bitwise-and-expression)))
      (production (:bitwise-and-expression-or-super :beta) (:super-expression) bitwise-and-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    
    (rule (:bitwise-xor-expression-or-super :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production (:bitwise-xor-expression-or-super :beta) ((:bitwise-xor-expression :beta)) bitwise-xor-expression-or-super-bitwise-xor-expression
        (validate (validate :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression)))
      (production (:bitwise-xor-expression-or-super :beta) (:super-expression) bitwise-xor-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    
    (rule (:bitwise-or-expression-or-super :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref-optional-limit)))
      (production (:bitwise-or-expression-or-super :beta) ((:bitwise-or-expression :beta)) bitwise-or-expression-or-super-bitwise-or-expression
        (validate (validate :bitwise-or-expression))
        (eval (eval :bitwise-or-expression)))
      (production (:bitwise-or-expression-or-super :beta) (:super-expression) bitwise-or-expression-or-super-super
        (validate (validate :super-expression))
        (eval (eval :super-expression))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Binary Logical Operators")
    (rule (:logical-and-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        ((validate cxt env) ((validate :bitwise-or-expression) cxt env))
        ((eval env mode) (return ((eval :bitwise-or-expression) env mode))))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((validate cxt env)
         ((validate :logical-and-expression) cxt env)
         ((validate :bitwise-or-expression) cxt env))
        ((eval env mode)
         (const ra obj-or-ref ((eval :logical-and-expression) env mode))
         (const a object (read-reference ra mode))
         (cond
          ((to-boolean a mode)
           (const rb obj-or-ref ((eval :bitwise-or-expression) env mode))
           (return (read-reference rb mode)))
          (nil (return a))))))
    
    (rule (:logical-xor-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        ((validate cxt env) ((validate :logical-and-expression) cxt env))
        ((eval env mode) (return ((eval :logical-and-expression) env mode))))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((validate cxt env)
         ((validate :logical-xor-expression) cxt env)
         ((validate :logical-and-expression) cxt env))
        ((eval env mode)
         (const ra obj-or-ref ((eval :logical-xor-expression) env mode))
         (const a object (read-reference ra mode))
         (const rb obj-or-ref ((eval :logical-and-expression) env mode))
         (const b object (read-reference rb mode))
         (const ba boolean (to-boolean a mode))
         (const bb boolean (to-boolean b mode))
         (return (xor ba bb)))))
    
    (rule (:logical-or-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        ((validate cxt env) ((validate :logical-xor-expression) cxt env))
        ((eval env mode) (return ((eval :logical-xor-expression) env mode))))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((validate cxt env)
         ((validate :logical-or-expression) cxt env)
         ((validate :logical-xor-expression) cxt env))
        ((eval env mode)
         (const ra obj-or-ref ((eval :logical-or-expression) env mode))
         (const a object (read-reference ra mode))
         (cond
          ((to-boolean a mode) (return a))
          (nil
           (const rb obj-or-ref ((eval :logical-xor-expression) env mode))
           (return (read-reference rb mode)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Conditional Operator")
    (rule (:conditional-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        ((validate cxt env) ((validate :logical-or-expression) cxt env))
        ((eval env mode) (return ((eval :logical-or-expression) env mode))))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((validate cxt env)
         ((validate :logical-or-expression) cxt env)
         ((validate :assignment-expression 1) cxt env)
         ((validate :assignment-expression 2) cxt env))
        ((eval env mode)
         (const ra obj-or-ref ((eval :logical-or-expression) env mode))
         (const a object (read-reference ra mode))
         (cond
          ((to-boolean a mode)
           (const rb obj-or-ref ((eval :assignment-expression 1) env mode))
           (return (read-reference rb mode)))
          (nil
           (const rc obj-or-ref ((eval :assignment-expression 2) env mode))
           (return (read-reference rc mode)))))))
    
    (rule (:non-assignment-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or
        ((validate cxt env) ((validate :logical-or-expression) cxt env))
        ((eval env mode) (return ((eval :logical-or-expression) env mode))))
      (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional
        ((validate cxt env)
         ((validate :logical-or-expression) cxt env)
         ((validate :non-assignment-expression 1) cxt env)
         ((validate :non-assignment-expression 2) cxt env))
        ((eval env mode)
         (const ra obj-or-ref ((eval :logical-or-expression) env mode))
         (const a object (read-reference ra mode))
         (cond
          ((to-boolean a mode)
           (const rb obj-or-ref ((eval :non-assignment-expression 1) env mode))
           (return (read-reference rb mode)))
          (nil
           (const rc obj-or-ref ((eval :non-assignment-expression 2) env mode))
           (return (read-reference rc mode)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Assignment Operators")
    (rule (:assignment-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        ((validate cxt env) ((validate :conditional-expression) cxt env))
        ((eval env mode) (return ((eval :conditional-expression) env mode))))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const ra obj-or-ref ((eval :postfix-expression) env mode))
         (const rb obj-or-ref ((eval :assignment-expression) env mode))
         (const b object (read-reference rb mode))
         (write-reference ra b mode)
         (return b)))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((validate cxt env)
         ((validate :postfix-expression-or-super) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (return (eval-assignment-op (table :compound-assignment) (eval :postfix-expression-or-super) (eval :assignment-expression) env mode))))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment :super-expression) assignment-expression-compound-super
        ((validate cxt env)
         ((validate :postfix-expression-or-super) cxt env)
         ((validate :super-expression) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (return (eval-assignment-op (table :compound-assignment) (eval :postfix-expression-or-super) (eval :super-expression) env mode))))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env mode)
         (rwhen (in mode (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r-left obj-or-ref ((eval :postfix-expression) env mode))
         (const o-left object (read-reference r-left mode))
         (const b-left boolean (to-boolean o-left mode))
         (var result object o-left)
         (case (operator :logical-assignment)
           (:select (tag and-eq)
             (when b-left
               (<- result (read-reference ((eval :assignment-expression) env mode) mode))))
           (:select (tag xor-eq)
             (const b-right boolean (to-boolean (read-reference ((eval :assignment-expression) env mode) mode) mode))
             (<- result (xor b-left b-right)))
           (:select (tag or-eq)
             (when (not b-left)
               (<- result (read-reference ((eval :assignment-expression) env mode) mode)))))
         (write-reference r-left result mode)
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
                                (left-eval (-> (environment mode) obj-or-ref-optional-limit))
                                (right-eval (-> (environment mode) obj-or-ref-optional-limit))
                                (env environment)
                                (mode (tag run))) obj-or-ref
      (const r-left obj-or-ref-optional-limit (left-eval env mode))
      (const o-left obj-optional-limit (read-ref-with-limit r-left mode))
      (const r-right obj-or-ref-optional-limit (right-eval env mode))
      (const o-right obj-optional-limit (read-ref-with-limit r-right mode))
      (const result object (binary-dispatch table o-left o-right mode))
      (write-reference r-left result mode)
      (return result))
    
    
    (%heading 2 "Comma Expressions")
    (rule (:list-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) obj-or-ref))
                                    (eval-as-list (-> (environment mode) (vector object))))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        ((validate cxt env) ((validate :assignment-expression) cxt env))
        ((eval env mode) (return ((eval :assignment-expression) env mode)))
        ((eval-as-list env mode)
         (const r obj-or-ref ((eval :assignment-expression) env mode))
         (const elt object (read-reference r mode))
         (return (vector elt))))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((validate cxt env)
         ((validate :list-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env mode)
         (const ra obj-or-ref ((eval :list-expression) env mode))
         (exec (read-reference ra mode))
         (const rb obj-or-ref ((eval :assignment-expression) env mode))
         (return (read-reference rb mode)))
        ((eval-as-list env mode)
         (const elts (vector object) ((eval-as-list :list-expression) env mode))
         (const r obj-or-ref ((eval :assignment-expression) env mode))
         (const elt object (read-reference r mode))
         (return (append elts (vector elt))))))
    
    (production :optional-expression ((:list-expression allow-in)) optional-expression-expression)
    (production :optional-expression () optional-expression-empty)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Type Expressions")
    (rule (:type-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment) class)))
      (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression
        ((validate cxt env) ((validate :non-assignment-expression) cxt env))
        ((eval env)
         (const r obj-or-ref ((eval :non-assignment-expression) env compile))
         (const o object (read-reference r compile))
         (rwhen (not-in o class :narrow-false)
           (throw type-error))
         (return o))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 1 "Statements")
    
    (grammar-argument :omega
                      abbrev       ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)        ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (rule (:statement :omega) ((validate (-> (context environment (list-set label)) void)) (eval (-> (environment object) object)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        ((validate cxt env (sl :unused)) ((validate :expression-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :expression-statement) env))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((validate cxt env (sl :unused)) ((validate :super-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :super-statement) env))))
      (production (:statement :omega) (:block) statement-block
        ((validate cxt env (sl :unused)) ((validate :block) cxt env))
        ((eval env d) (return ((eval :block) env d))))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        ((validate cxt env sl) ((validate :labeled-statement) cxt env sl))
        ((eval env d) (return ((eval :labeled-statement) env d))))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        ((validate cxt env (sl :unused)) ((validate :if-statement) cxt env))
        ((eval env d) (return ((eval :if-statement) env d))))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((validate (cxt :unused) (env :unused) (sl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((validate cxt env sl) ((validate :do-statement) cxt env sl))
        ((eval env d) (return ((eval :do-statement) env d))))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((validate cxt env sl) ((validate :while-statement) cxt env sl))
        ((eval env d) (return ((eval :while-statement) env d))))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((validate (cxt :unused) (env :unused) (sl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((validate (cxt :unused) (env :unused) (sl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        ((validate cxt (env :unused) (sl :unused)) ((validate :continue-statement) cxt))
        ((eval env d) (return ((eval :continue-statement) env d))))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        ((validate cxt (env :unused) (sl :unused)) ((validate :break-statement) cxt))
        ((eval env d) (return ((eval :break-statement) env d))))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        ((validate cxt env (sl :unused)) ((validate :return-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :return-statement) env))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((validate cxt env (sl :unused)) ((validate :throw-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :throw-statement) env))))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((validate (cxt :unused) (env :unused) (sl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo))))
    
    
    (rule (:substatement :omega) ((enabled (writable-cell boolean)) (validate (-> (context environment (list-set label)) void))
                                  (eval (-> (environment object) object)))
      (production (:substatement :omega) (:empty-statement) substatement-empty-statement
        ((validate (cxt :unused) (env :unused) (sl :unused)))
        ((eval (env :unused) d) (return d)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        ((validate cxt env sl) ((validate :statement) cxt env sl))
        ((eval env d) (return ((eval :statement) env d))))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((validate cxt env (sl :unused)) ((validate :simple-variable-definition) cxt env))
        ((eval env d) (return ((eval :simple-variable-definition) env d))))
      (production (:substatement :omega) (:attributes :no-line-break { :substatements }) substatement-annotated-group
        ((validate cxt env (sl :unused))
         ((validate :attributes) cxt env)
         (const a attribute ((eval :attributes) env compile))
         (rwhen (not-in a boolean :narrow-false)
           (throw type-error))
         (action<- (enabled :substatement 0) a)
         (when a
           ((validate :substatements) cxt env)))
        ((eval env d)
         (if (enabled :substatement 0)
           (return ((eval :substatements) env d))
           (return d)))))
    
    
    (rule :substatements ((validate (-> (context environment) void)) (eval (-> (environment object) object)))
      (production :substatements () substatements-none
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) d) (return d)))
      (production :substatements (:substatements-prefix (:substatement abbrev)) substatements-more
        ((validate cxt env)
         ((validate :substatements-prefix) cxt env)
         ((validate :substatement) cxt env (list-set-of label)))
        ((eval env d)
         (const o object ((eval :substatements-prefix) env d))
         (return ((eval :substatement) env o)))))
    
    (rule :substatements-prefix ((validate (-> (context environment) void)) (eval (-> (environment object) object)))
      (production :substatements-prefix () substatements-prefix-none
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) d) (return d)))
      (production :substatements-prefix (:substatements-prefix (:substatement full)) substatements-prefix-more
        ((validate cxt env)
         ((validate :substatements-prefix) cxt env)
         ((validate :substatement) cxt env (list-set-of label)))
        ((eval env d)
         (const o object ((eval :substatements-prefix) env d))
         (return ((eval :substatement) env o)))))
    
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon no-short-if) () semicolon-no-short-if)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%heading 2 "Expression Statement")
    (rule :expression-statement ((validate (-> (context environment) void)) (eval (-> (environment) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        ((validate cxt env) ((validate :list-expression) cxt env))
        ((eval env)
         (const r obj-or-ref ((eval :list-expression) env run))
         (return (read-reference r run)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Super Statement")
    (rule :super-statement ((validate (-> (context environment) void)) (eval (-> (environment) object)))
      (production :super-statement (super :arguments) super-statement-super-arguments
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval (env :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Block Statement")
    (rule :block ((compile-frame (writable-cell block-frame)) (validate (-> (context environment) void)) (eval (-> (environment object) object)))
      (production :block ({ :directives }) block-directives
        ((validate cxt env)
         (const compile-frame block-frame (new block-frame (list-set-of binding) (list-set-of binding)))
         (action<- (compile-frame :block 0) compile-frame)
         (exec ((validate :directives) cxt (cons compile-frame env) none)))
        ((eval env d)
         (const compile-frame block-frame (compile-frame :block 0))
         (const runtime-frame block-frame (new block-frame (copy-bindings (& read-bindings compile-frame)) (copy-bindings (& write-bindings compile-frame))))
         (return ((eval :directives) (cons runtime-frame env) d)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Labeled Statements")
    (rule (:labeled-statement :omega) ((validate (-> (context environment (list-set label)) void)) (eval (-> (environment object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((validate cxt env sl)
         (const name string (name :identifier))
         (rwhen (set-in name (& break-labels cxt))
           (throw syntax-error))
         (const cxt2 context (add-break-label cxt name))
         ((validate :substatement) cxt2 env (set+ sl (list-set-of label name))))
        ((eval env d)
         (catch ((return ((eval :substatement) env d)))
           (x) (if (and (in x break :narrow-true) (= (& label x) (name :identifier) label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "If Statement")
    (rule (:if-statement :omega) ((validate (-> (context environment) void)) (eval (-> (environment object) object)))
      (production (:if-statement abbrev) (if :paren-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((validate cxt env)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label)))
        ((eval env d)
         (const r obj-or-ref ((eval :paren-list-expression) env run))
         (const o object (read-reference r run))
         (if (to-boolean o run)
           (return ((eval :substatement) env d))
           (return d))))
      (production (:if-statement full) (if :paren-list-expression (:substatement full)) if-statement-if-then-full
        ((validate cxt env)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label)))
        ((eval env d)
         (const r obj-or-ref ((eval :paren-list-expression) env run))
         (const o object (read-reference r run))
         (if (to-boolean o run)
           (return ((eval :substatement) env d))
           (return d))))
      (production (:if-statement :omega) (if :paren-list-expression (:substatement no-short-if) else (:substatement :omega))
                  if-statement-if-then-else
        ((validate cxt env)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement 1) cxt env (list-set-of label))
         ((validate :substatement 2) cxt env (list-set-of label)))
        ((eval env d)
         (const r obj-or-ref ((eval :paren-list-expression) env run))
         (const o object (read-reference r run))
         (if (to-boolean o run)
           (return ((eval :substatement 1) env d))
           (return ((eval :substatement 2) env d))))))
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
    (rule :do-statement ((labels (writable-cell (list-set label))) (validate (-> (context environment (list-set label)) void))
                         (eval (-> (environment object) object)))
      (production :do-statement (do (:substatement abbrev) while :paren-list-expression) do-statement-do-while
        ((validate cxt env sl)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :do-statement 0) continue-labels)
         (const cxt2 context (add-break-label cxt default))
         (const cxt3 context (add-continue-labels cxt2 continue-labels))
         ((validate :substatement) cxt3 env (list-set-of label))
         ((validate :paren-list-expression) cxt env))
        ((eval env d)
         (catch ((var d1 object d)
                 (while true
                   (catch ((<- d1 ((eval :substatement) env d1)))
                     (x) (if (and (in x continue :narrow-true) (set-in (& label x) (labels :do-statement 0)))
                           (<- d1 (& value x))
                           (throw x)))
                   (const r obj-or-ref ((eval :paren-list-expression) env run))
                   (const o object (read-reference r run))
                   (rwhen (not (to-boolean o run))
                     (return d1))))
           (x) (if (and (in x break :narrow-true) (= (& label x) default label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" labels validate) ("Evaluation" eval))
    
    
    (%heading 2 "While Statement")
    (rule (:while-statement :omega) ((labels (writable-cell (list-set label))) (validate (-> (context environment (list-set label)) void))
                                     (eval (-> (environment object) object)))
      (production (:while-statement :omega) (while :paren-list-expression (:substatement :omega)) while-statement-while
        ((validate cxt env sl)
         ((validate :paren-list-expression) cxt env)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :while-statement 0) continue-labels)
         (const cxt2 context (add-break-label cxt default))
         (const cxt3 context (add-continue-labels cxt2 continue-labels))
         ((validate :substatement) cxt3 env (list-set-of label)))
        ((eval env d)
         (catch ((var d1 object d)
                 (while (to-boolean (read-reference ((eval :paren-list-expression) env run) run) run)
                   (catch ((<- d1 ((eval :substatement) env d1)))
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
    (rule :continue-statement ((validate (-> (context) void)) (eval (-> (environment object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((validate cxt)
         (rwhen (set-not-in default (& continue-labels cxt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new continue d default))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((validate cxt)
         (rwhen (set-not-in (name :identifier) (& continue-labels cxt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new continue d (name :identifier))))))
    
    (rule :break-statement ((validate (-> (context) void)) (eval (-> (environment object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((validate cxt)
         (rwhen (set-not-in default (& break-labels cxt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new break d default))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((validate cxt)
         (rwhen (set-not-in (name :identifier) (& break-labels cxt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new break d (name :identifier))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Return Statement")
    (rule :return-statement ((validate (-> (context environment) void)) (eval (-> (environment) object)))
      (production :return-statement (return) return-statement-default
        ((validate cxt (env :unused))
         (rwhen (not (& inside-function cxt))
           (throw syntax-error)))
        ((eval (env :unused)) (throw (new returned-value undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((validate cxt env)
         (rwhen (not (& inside-function cxt))
           (throw syntax-error))
         ((validate :list-expression) cxt env))
        ((eval env)
         (const r obj-or-ref ((eval :list-expression) env run))
         (const a object (read-reference r run))
         (throw (new returned-value a)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Throw Statement")
    (rule :throw-statement ((validate (-> (context environment) void)) (eval (-> (environment) object)))
      (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw
        (validate (validate :list-expression))
        ((eval env)
         (const r obj-or-ref ((eval :list-expression) env run))
         (const a object (read-reference r run))
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
    (rule (:directive :omega_2) ((enabled (writable-cell boolean)) (validate (-> (context environment attribute-opt-not-false) context))
                                 (eval (-> (environment object) object)))
      (production (:directive :omega_2) (:empty-statement) directive-empty-statement
        ((validate cxt (env :unused) (a :unused)) (return cxt))
        ((eval (env :unused) d) (return d)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        ((validate cxt env a)
         (rwhen (not-in a (tag none true))
           (throw syntax-error))
         ((validate :statement) cxt env (list-set-of label))
         (return cxt))
        ((eval env d) (return ((eval :statement) env d))))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((validate cxt env a) (return ((validate :annotatable-directive) cxt env a)))
        ((eval env d) (return ((eval :annotatable-directive) env d))))
      (production (:directive :omega_2) (:attributes :no-line-break (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((validate cxt env a)
         ((validate :attributes) cxt env)
         (const a2 attribute ((eval :attributes) env compile))
         (const a3 attribute (combine-attributes a a2))
         (action<- (enabled :directive 0) (not-in a3 false-type))
         (if (not-in a3 false-type :narrow-true)
           (return ((validate :annotatable-directive) cxt env a3))
           (return cxt)))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :annotatable-directive) env d))
           (return d))))
      (production (:directive :omega_2) (:attributes :no-line-break { :directives }) directive-annotated-group
        ((validate cxt env a)
         ((validate :attributes) cxt env)
         (const a2 attribute ((eval :attributes) env compile))
         (const a3 attribute (combine-attributes a a2))
         (action<- (enabled :directive 0) (not-in a3 false-type))
         (rwhen (in a3 false-type :narrow-false)
           (return cxt))
         (return ((validate :directives) cxt env a3)))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :directives) env d))
           (return d))))
      (production (:directive :omega_2) (:package-definition) directive-package-definition
        ((validate (cxt :unused) (env :unused) a) (if (in a (tag none true)) (todo) (throw syntax-error)))
        ((eval (env :unused) (d :unused)) (todo)))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((validate (cxt :unused) (env :unused) a) (if (in a (tag none true)) (todo) (throw syntax-error)))
          ((eval (env :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((validate cxt (env :unused) (a :unused)) (return ((validate :pragma) cxt)))
        ((eval (env :unused) d) (return d))))
    
    (rule (:annotatable-directive :omega_2) ((validate (-> (context environment attribute-opt-not-false) context)) (eval (-> (environment object) object)))
      (production (:annotatable-directive :omega_2) (:export-definition (:semicolon :omega_2)) annotatable-directive-export-definition
        ((validate (cxt :unused) (env :unused) (a :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:variable-definition (:semicolon :omega_2)) annotatable-directive-variable-definition
        ((validate cxt env a)
         ((validate :variable-definition) cxt env a)
         (return cxt))
        ((eval env d) (return ((eval :variable-definition) env d))))
      (production (:annotatable-directive :omega_2) ((:function-definition :omega_2)) annotatable-directive-function-definition
        ((validate (cxt :unused) (env :unused) (a :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) ((:class-definition :omega_2)) annotatable-directive-class-definition
        ((validate (cxt :unused) (env :unused) (a :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:namespace-definition (:semicolon :omega_2)) annotatable-directive-namespace-definition
        ((validate (cxt :unused) (env :unused) (a :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (? js2
        (production (:annotatable-directive :omega_2) ((:interface-definition :omega_2)) annotatable-directive-interface-definition
          ((validate (cxt :unused) (env :unused) (a :unused)) (todo))
          ((eval (env :unused) (d :unused)) (todo))))
      (production (:annotatable-directive :omega_2) (:import-directive (:semicolon :omega_2)) annotatable-directive-import-directive
        ((validate (cxt :unused) (env :unused) (a :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:use-directive (:semicolon :omega_2)) annotatable-directive-use-directive
        ((validate (cxt :unused) (env :unused) (a :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo))))
    
    
    (rule :directives ((validate (-> (context environment attribute-opt-not-false) context)) (eval (-> (environment object) object)))
      (production :directives () directives-none
        ((validate cxt (env :unused) (a :unused)) (return cxt))
        ((eval (env :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((validate cxt env a)
         (const cxt2 context ((validate :directives-prefix) cxt env a))
         (return ((validate :directive) cxt2 env a)))
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    
    (rule :directives-prefix ((validate (-> (context environment attribute-opt-not-false) context)) (eval (-> (environment object) object)))
      (production :directives-prefix () directives-prefix-none
        ((validate cxt (env :unused) (a :unused)) (return cxt))
        ((eval (env :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((validate cxt env a)
         (const cxt2 context ((validate :directives-prefix) cxt env a))
         (return ((validate :directive) cxt2 env a)))
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Attributes")
    (rule :attributes ((validate (-> (context environment) void)) (eval (-> (environment mode) attribute)))
      (production :attributes (:attribute) attributes-one
        ((validate cxt env) ((validate :attribute) cxt env))
        ((eval env mode) (return ((eval :attribute) env mode))))
      (production :attributes (:attribute-combination) attributes-attribute-combination
        ((validate cxt env) ((validate :attribute-combination) cxt env))
        ((eval env mode) (return ((eval :attribute-combination) env mode)))))
    
    
    (rule :attribute-combination ((validate (-> (context environment) void)) (eval (-> (environment mode) attribute)))
      (production :attribute-combination (:attribute :no-line-break :attributes) attribute-combination-more
        ((validate cxt env)
         ((validate :attribute) cxt env)
         ((validate :attributes) cxt env))
        ((eval env mode)
         (const a attribute ((eval :attribute) env mode))
         (rwhen (in a false-type :narrow-false)
           (return false))
         (const b attribute ((eval :attributes) env mode))
         (return (combine-attributes a b)))))
    
    
    (rule :attribute ((validate (-> (context environment) void)) (eval (-> (environment mode) attribute)))
      (production :attribute (:attribute-expression) attribute-attribute-expression
        ((validate cxt env) ((validate :attribute-expression) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :attribute-expression) env mode))
         (const a object (read-reference r mode))
         (rwhen (not-in a attribute :narrow-false)
           (throw type-error))
         (return a)))
      (production :attribute (true) attribute-true
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return true)))
      (production :attribute (false) attribute-false
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return false)))
      (production :attribute (public) attribute-public
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (mode :unused)) (return public-namespace)))
      (production :attribute (:nonexpression-attribute) attribute-nonexpression-attribute
        ((validate (cxt :unused) env) ((validate :nonexpression-attribute) env))
        ((eval env mode) (return ((eval :nonexpression-attribute) env mode)))))
    
    
    (rule :nonexpression-attribute ((validate (-> (environment) void)) (eval (-> (environment mode) attribute)))
      (production :nonexpression-attribute (abstract) nonexpression-attribute-abstract
        ((validate (env :unused)))
        ((eval (env :unused) (mode :unused)) (return (new compound-attribute (list-set-of namespace) none false false false abstract none false false))))
      (production :nonexpression-attribute (final) nonexpression-attribute-final
        ((validate (env :unused)))
        ((eval (env :unused) (mode :unused)) (return (new compound-attribute (list-set-of namespace) none false false false final none false false))))
      (production :nonexpression-attribute (private) nonexpression-attribute-private
        ((validate env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw syntax-error)))
        ((eval env (mode :unused))
         (const c class-opt (get-enclosing-class env))
         (assert (not-in c (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local c) " cannot be " (:tag none) " at this point.")
         (return (& private-namespace c))))
      (production :nonexpression-attribute (static) nonexpression-attribute-static
        ((validate (env :unused)))
        ((eval (env :unused) (mode :unused)) (return (new compound-attribute (list-set-of namespace) none false false false static none false false)))))
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
    (rule :pragma ((validate (-> (context) context)))
      (production :pragma (use :pragma-items) pragma-pragma-items
        ((validate cxt) (return ((validate :pragma-items) cxt)))))
    
    (rule :pragma-items ((validate (-> (context) context)))
      (production :pragma-items (:pragma-item) pragma-items-one
        ((validate cxt) (return ((validate :pragma-item) cxt))))
      (production :pragma-items (:pragma-items \, :pragma-item) pragma-items-more
        ((validate cxt)
         (const cxt2 context ((validate :pragma-items) cxt))
         (return ((validate :pragma-item) cxt2)))))
    
    (rule :pragma-item ((validate (-> (context) context)))
      (production :pragma-item (:pragma-expr) pragma-item-pragma-expr
        ((validate cxt) (return ((validate :pragma-expr) cxt false))))
      (production :pragma-item (:pragma-expr \?) pragma-item-optional-pragma-expr
        ((validate cxt) (return ((validate :pragma-expr) cxt true)))))
    
    (rule :pragma-expr ((validate (-> (context boolean) context)))
      (production :pragma-expr (:identifier) pragma-expr-identifier
        ((validate cxt optional)
         (return (process-pragma cxt (name :identifier) undefined optional))))
      (production :pragma-expr (:identifier \( :pragma-argument \)) pragma-expr-identifier-and-parameter
        ((validate cxt optional)
         (const arg object (value :pragma-argument))
         (return (process-pragma cxt (name :identifier) arg optional)))))
    
    (rule :pragma-argument ((value object))
      (production :pragma-argument (true) pragma-argument-true (value true))
      (production :pragma-argument (false) pragma-argument-false (value false))
      (production :pragma-argument ($number) pragma-argument-number (value (value $number)))
      (production :pragma-argument (- $number) pragma-argument-negative-number (value (float64-negate (value $number))))
      (production :pragma-argument ($string) pragma-argument-string (value (value $string))))
    (%print-actions ("Validation" validate))
    
    (define (process-pragma (cxt context) (name string) (value object) (optional boolean)) context
      (when (= name "strict" string)
        (rwhen (in value (tag true undefined) :narrow-false)
          (return (new context true (& wrap cxt) (& inside-function cxt) (& open-namespaces cxt) (& break-labels cxt) (& continue-labels cxt))))
        (rwhen (in value (tag false))
          (return (new context false (& wrap cxt) (& inside-function cxt) (& open-namespaces cxt) (& break-labels cxt) (& continue-labels cxt)))))
      (when (= name "wrap" string)
        (rwhen (in value (tag true undefined) :narrow-false)
          (return (new context (& strict cxt) true (& inside-function cxt) (& open-namespaces cxt) (& break-labels cxt) (& continue-labels cxt))))
        (rwhen (in value (tag false))
          (return (new context (& strict cxt) false (& inside-function cxt) (& open-namespaces cxt) (& break-labels cxt) (& continue-labels cxt)))))
      (when (= name "ecmascript" string)
        (rwhen (set-in value (list-set-of object undefined 4.0))
          (return cxt))
        (rwhen (set-in value (list-set-of object 1.0 2.0 3.0))
          (// "An implementation may optionally modify " (:local cxt) " to disable features not available in ECMAScript Edition " (:local value)
              " other than subsequent pragmas.")
          (return cxt)))
      (if optional
        (return cxt)
        (throw type-error)))
    
    
    (%heading 1 "Definitions")
    (%heading 2 "Export Definition")
    (production :export-definition (export :export-binding-list) export-definition-definition)
    
    (production :export-binding-list (:export-binding) export-binding-list-one)
    (production :export-binding-list (:export-binding-list \, :export-binding) export-binding-list-more)
    
    (production :export-binding (:function-name) export-binding-simple)
    (production :export-binding (:function-name = :function-name) export-binding-initialiser)
    
    
    (%heading 2 "Variable Definition")
    (rule :variable-definition ((validate (-> (context environment attribute-opt-not-false) void)) (eval (-> (environment object) object)))
      (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition
        ((validate cxt env a)
         (const access (tag read read-write) (access :variable-definition-kind))
         ((validate :variable-binding-list) cxt env a access))
        ((eval env d)
         (const access (tag read read-write) (access :variable-definition-kind))
         ((eval :variable-binding-list) env access)
         (return d))))
    
    (rule :variable-definition-kind ((access (tag read read-write)))
      (production :variable-definition-kind (var) variable-definition-kind-var (access read-write))
      (production :variable-definition-kind (const) variable-definition-kind-const (access read)))
    
    (rule (:variable-binding-list :beta) ((validate (-> (context environment attribute-opt-not-false (tag read read-write)) void))
                                          (eval (-> (environment (tag read read-write)) void)))
      (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one
        ((validate cxt env a access) ((validate :variable-binding) cxt env a access))
        ((eval env access) ((eval :variable-binding) env access)))
      (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more
        ((validate cxt env a access)
         ((validate :variable-binding-list) cxt env a access)
         ((validate :variable-binding) cxt env a access))
        ((eval env access)
         ((eval :variable-binding-list) env access)
         ((eval :variable-binding) env access))))
    
    (rule (:variable-binding :beta) ((hoisted (writable-cell boolean)) (attributes (writable-cell attribute-opt-not-false))
                                     (validate (-> (context environment attribute-opt-not-false (tag read read-write)) void))
                                     (eval (-> (environment (tag read read-write)) void)))
      (production (:variable-binding :beta) ((:typed-identifier :beta)) variable-binding-simple
        ((validate cxt env a access)
         (const hoisted boolean (and (not (& strict cxt)) (in (get-regional-frame env) (union package-frame function-frame))
                                     (= access read-write (tag read read-write)) (in a (tag none)) (not (type-present :typed-identifier))))
         (action<- (hoisted :variable-binding 0) hoisted)
         (action<- (attributes :variable-binding 0) a)
         ((validate :typed-identifier) cxt env)
         (exec (define-variable cxt env access hoisted a (name :typed-identifier))))
        ((eval env access)
         (const t class-opt ((eval :typed-identifier) env))
         (create-variable env access (hoisted :variable-binding 0) (attributes :variable-binding 0) (name :typed-identifier) t none)))
      (production (:variable-binding :beta) ((:typed-identifier :beta) = (:variable-initialiser :beta)) variable-binding-initialised
        ((validate cxt env a access)
         (const hoisted boolean (and (not (& strict cxt)) (in (get-regional-frame env) (union package-frame function-frame))
                                     (= access read-write (tag read read-write)) (in a (tag none)) (not (type-present :typed-identifier))))
         (action<- (hoisted :variable-binding 0) hoisted)
         (action<- (attributes :variable-binding 0) a)
         ((validate :typed-identifier) cxt env)
         ((validate :variable-initialiser) cxt env)
         (exec (define-variable cxt env access hoisted a (name :typed-identifier))))
        ((eval env access)
         (const t class-opt ((eval :typed-identifier) env))
         (const o object ((eval :variable-initialiser) env run))
         (create-variable env access (hoisted :variable-binding 0) (attributes :variable-binding 0) (name :typed-identifier) t o))))
    
    (rule (:variable-initialiser :beta) ((validate (-> (context environment) void)) (eval (-> (environment mode) object)))
      (production (:variable-initialiser :beta) ((:assignment-expression :beta)) variable-initialiser-assignment-expression
        ((validate cxt env) ((validate :assignment-expression) cxt env))
        ((eval env mode)
         (const r obj-or-ref ((eval :assignment-expression) env mode))
         (return (read-reference r mode))))
      (production (:variable-initialiser :beta) (:nonexpression-attribute) variable-initialiser-nonexpression-attribute
        ((validate (cxt :unused) env) ((validate :nonexpression-attribute) env))
        ((eval env mode) (return ((eval :nonexpression-attribute) env mode))))
      (production (:variable-initialiser :beta) (:attribute-combination) variable-initialiser-attribute-combination
        ((validate cxt env) ((validate :attribute-combination) cxt env))
        ((eval env mode) (return ((eval :attribute-combination) env mode)))))
    
    
    (rule (:typed-identifier :beta) ((name string) (type-present boolean) (validate (-> (context environment) void)) (eval (-> (environment) class-opt)))
      (production (:typed-identifier :beta) (:identifier) typed-identifier-identifier
        (name (name :identifier))
        (type-present false)
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused)) (return none)))
      (production (:typed-identifier :beta) (:identifier \: (:type-expression :beta)) typed-identifier-identifier-and-type
        (name (name :identifier))
        (type-present true)
        ((validate cxt env) ((validate :type-expression) cxt env))
        ((eval env) (return ((eval :type-expression) env)))))
    ;(production (:typed-identifier :beta) ((:type-expression :beta) :identifier) typed-identifier-type-and-identifier)
    (%print-actions ("Validation" name type-present access hoisted attributes validate) ("Evaluation" eval))
    
    
    (%heading 2 "Simple Variable Definition")
    (%text :syntax "A " (:grammar-symbol :simple-variable-definition) " represents the subset of " (:grammar-symbol :variable-definition)
           " expansions that may be used when the variable definition is used as a " (:grammar-symbol (:substatement :omega))
           " instead of a " (:grammar-symbol (:directive :omega_2)) " in non-strict mode. "
           "In strict mode variable definitions may not be used as substatements.")
    (rule :simple-variable-definition ((validate (-> (context environment) void)) (eval (-> (environment object) object)))
      (production :simple-variable-definition (var :untyped-variable-binding-list) simple-variable-definition-definition
        ((validate cxt env)
         (rwhen (or (& strict cxt) (not-in (get-regional-frame env) (union package-frame function-frame)))
           (throw syntax-error))
         ((validate :untyped-variable-binding-list) cxt env))
        ((eval env d)
         ((eval :untyped-variable-binding-list) env)
         (return d))))
    
    (rule :untyped-variable-binding-list ((validate (-> (context environment) void)) (eval (-> (environment) void)))
      (production :untyped-variable-binding-list (:untyped-variable-binding) untyped-variable-binding-list-one
        ((validate cxt env) ((validate :untyped-variable-binding) cxt env))
        ((eval env) ((eval :untyped-variable-binding) env)))
      (production :untyped-variable-binding-list (:untyped-variable-binding-list \, :untyped-variable-binding) untyped-variable-binding-list-more
        ((validate cxt env)
         ((validate :untyped-variable-binding-list) cxt env)
         ((validate :untyped-variable-binding) cxt env))
        ((eval env)
         ((eval :untyped-variable-binding-list) env)
         ((eval :untyped-variable-binding) env))))
    
    (rule :untyped-variable-binding ((validate (-> (context environment) void)) (eval (-> (environment) void)))
      (production :untyped-variable-binding (:identifier) untyped-variable-binding-simple
        ((validate cxt env)
         (exec (define-variable cxt env read-write true none (name :identifier))))
        ((eval env)
         (create-variable env read-write true none (name :identifier) none none)))
      (production :untyped-variable-binding (:identifier = (:variable-initialiser allow-in)) untyped-variable-binding-initialised
        ((validate cxt env)
         ((validate :variable-initialiser) cxt env)
         (exec (define-variable cxt env read-write true none (name :identifier))))
        ((eval env)
         (const o object ((eval :variable-initialiser) env run))
         (create-variable env read-write true none (name :identifier) none o))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
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
          (exec ((validate :directives) initial-context initial-environment none))
          (return ((eval :directives) initial-environment undefined))))))
    
    
    
    (%heading (1 :semantics) "Predefined Identifiers")
    (%heading (1 :semantics) "Built-in Classes")
    (%heading (1 :semantics) "Built-in Functions")
    (%heading (1 :semantics) "Built-in Attributes")
    (%heading (1 :semantics) "Built-in Operators")
    (%heading (2 :semantics) "Unary Operators")
    
    (define (plus-object (this object :unused) (a object) (args argument-list :unused) (mode mode)) object
      (return (to-number a mode)))
    
    (define (minus-object (this object :unused) (a object) (args argument-list :unused) (mode mode)) object
      (return (float64-negate (to-number a mode))))
    
    (define (bitwise-not-object (this object :unused) (a object) (args argument-list :unused) (mode mode)) object
      (const i integer (to-int32 (to-number a mode)))
      (return (real-to-float64 (bitwise-xor i -1))))
    
    (define (increment-object (this object :unused) (a object) (args argument-list :unused) (mode mode)) object
      (const x object (unary-plus a mode))
      (return (binary-dispatch add-table x 1.0 mode)))
    
    (define (decrement-object (this object :unused) (a object) (args argument-list :unused) (mode mode)) object
      (const x object (unary-plus a mode))
      (return (binary-dispatch subtract-table x 1.0 mode)))
    
    (define (call-object (this object) (a object) (args argument-list) (mode mode)) object
      (case a
        (:select (union undefined null boolean float64 string namespace compound-attribute prototype) (throw type-error))
        (:narrow (union class instance) (return ((& call a) this args mode)))
        (:narrow method-closure (return (call-object (& this a) (& code (& method a)) args mode)))))
    
    (define (construct-object (this object) (a object) (args argument-list) (mode mode)) object
      (case a
        (:select (union undefined null boolean float64 string namespace compound-attribute method-closure prototype) (throw type-error))
        (:narrow (union class instance) (return ((& construct a) this args mode)))))
    
    (define (bracket-read-object (this object :unused) (a object) (args argument-list) (mode mode)) object
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0) mode))
      (return (read-property a (list-set (new qualified-name public-namespace name)) (list-set-of visibility-modifier indexable enumerable) mode)))
    
    (define (bracket-write-object (this object :unused) (a object) (args argument-list) (mode mode)) object
      (rwhen (in mode (tag compile) :narrow-false)
        (throw compile-expression-error))
      (rwhen (or (/= (length (& positional args)) 2) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const new-value object (nth (& positional args) 0))
      (const name string (to-string (nth (& positional args) 1) mode))
      (write-property a (list-set (new qualified-name public-namespace name)) (list-set-of visibility-modifier indexable enumerable) new-value mode)
      (return undefined))
    
    (define (bracket-delete-object (this object :unused) (a object) (args argument-list) (mode mode)) object
      (rwhen (in mode (tag compile) :narrow-false)
        (throw compile-expression-error))
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0) mode))
      (return (delete-qualified-property a name public-namespace (list-set-of visibility-modifier indexable enumerable) mode)))
    
    
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
    
    (define (add-objects (a object) (b object) (mode mode)) object
      (const ap object (to-primitive a null mode))
      (const bp object (to-primitive b null mode))
      (if (or (in ap string) (in bp string))
        (return (append (to-string ap mode) (to-string bp mode)))
        (return (float64-add (to-number ap mode) (to-number bp mode)))))
    
    (define (subtract-objects (a object) (b object) (mode mode)) object
      (return (float64-subtract (to-number a mode) (to-number b mode))))
    
    (define (multiply-objects (a object) (b object) (mode mode)) object
      (return (float64-multiply (to-number a mode) (to-number b mode))))
    
    (define (divide-objects (a object) (b object) (mode mode)) object
      (return (float64-divide (to-number a mode) (to-number b mode))))
    
    (define (remainder-objects (a object) (b object) (mode mode)) object
      (return (float64-remainder (to-number a mode) (to-number b mode))))
    
    
    (define (less-objects (a object) (b object) (mode mode)) object
      (const ap object (to-primitive a null mode))
      (const bp object (to-primitive b null mode))
      (if (and (in ap string :narrow-true) (in bp string :narrow-true))
        (return (< ap bp string))
        (return (= (float64-compare (to-number ap mode) (to-number bp mode)) less order))))
    
    (define (less-or-equal-objects (a object) (b object) (mode mode)) object
      (const ap object (to-primitive a null mode))
      (const bp object (to-primitive b null mode))
      (if (and (in ap string :narrow-true) (in bp string :narrow-true))
        (return (<= ap bp string))
        (return (in (float64-compare (to-number ap mode) (to-number bp mode)) (tag less equal)))))
    
    (define (equal-objects (a object) (b object) (mode mode)) object
      (case a
        (:select (union undefined null)
          (return (in b (union undefined null))))
        (:narrow boolean
          (if (in b boolean :narrow-true)
            (return (= a b boolean))
            (return (equal-objects (to-number a mode) b mode))))
        (:narrow float64
          (const bp object (to-primitive b null mode))
          (case bp
            (:select (union undefined null namespace compound-attribute class method-closure prototype instance) (return false))
            (:select (union boolean string float64) (return (= (float64-compare a (to-number bp mode)) equal order)))))
        (:narrow string
          (const bp object (to-primitive b null mode))
          (case bp
            (:select (union undefined null namespace compound-attribute class method-closure prototype instance) (return false))
            (:select (union boolean float64) (return (= (float64-compare (to-number a mode) (to-number bp mode)) equal order)))
            (:narrow string (return (= a bp string)))))
        (:select (union namespace compound-attribute class method-closure prototype instance)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace compound-attribute class method-closure prototype instance) (return (strict-equal-objects a b mode)))
            (:select (union boolean float64 string)
              (const ap object (to-primitive a null mode))
              (case ap
                (:select (union undefined null namespace compound-attribute class method-closure prototype instance) (return false))
                (:select (union boolean float64 string) (return (equal-objects ap b mode)))))))))
    
    (define (strict-equal-objects (a object) (b object) (mode mode :unused)) object
      (if (and (in a float64 :narrow-true) (in b float64 :narrow-true))
        (return (= (float64-compare a b) equal order))
        (return (= a b object))))
    
    
    (define (shift-left-objects (a object) (b object) (mode mode)) object
      (const i integer (to-u-int32 (to-number a mode)))
      (const count integer (bitwise-and (to-u-int32 (to-number b mode)) (hex #x1F)))
      (return (real-to-float64 (u-int32-to-int32 (bitwise-and (bitwise-shift i count) (hex #xFFFFFFFF))))))
    
    (define (shift-right-objects (a object) (b object) (mode mode)) object
      (const i integer (to-int32 (to-number a mode)))
      (const count integer (bitwise-and (to-u-int32 (to-number b mode)) (hex #x1F)))
      (return (real-to-float64 (bitwise-shift i (neg count)))))
    
    (define (shift-right-unsigned-objects (a object) (b object) (mode mode)) object
      (const i integer (to-u-int32 (to-number a mode)))
      (const count integer (bitwise-and (to-u-int32 (to-number b mode)) (hex #x1F)))
      (return (real-to-float64 (bitwise-shift i (neg count)))))
    
    (define (bitwise-and-objects (a object) (b object) (mode mode)) object
      (const i integer (to-int32 (to-number a mode)))
      (const j integer (to-int32 (to-number b mode)))
      (return (real-to-float64 (bitwise-and i j))))
    
    (define (bitwise-xor-objects (a object) (b object) (mode mode)) object
      (const i integer (to-int32 (to-number a mode)))
      (const j integer (to-int32 (to-number b mode)))
      (return (real-to-float64 (bitwise-xor i j))))
    
    (define (bitwise-or-objects (a object) (b object) (mode mode)) object
      (const i integer (to-int32 (to-number a mode)))
      (const j integer (to-int32 (to-number b mode)))
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
