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
    (deftag range-error)
    (deftag bad-value-error)
    (deftag property-access-error)
    (deftag definition-error)
    (deftag argument-mismatch-error)
    (deftype semantic-error (tag syntax-error compile-expression-error reference-error uninitialised-error range-error bad-value-error property-access-error
                                 definition-error argument-mismatch-error))
    
    (deftuple break (value object) (label label))
    (deftuple continue (value object) (label label))
    (deftuple returned-value (value object))
    (deftuple thrown-value (value object))
    (deftype early-exit (union break continue returned-value thrown-value))
    
    (deftype semantic-exception (union early-exit semantic-error))
    
    
    (%heading (2 :semantics) "Objects")
    (deftag none)
    (deftag ok)
    (deftag inaccessible)
    (deftag uninitialised)
    
    (deftype object (union undefined null boolean long u-long float32 float64 character string namespace compound-attribute class method-closure prototype instance package global))
    (deftype primitive-object (union undefined null boolean long u-long float32 float64 character string))
    (deftype dynamic-object (union prototype dynamic-instance global))
    
    (deftype object-opt (union object (tag none)))
    (deftype object-i (union object (tag inaccessible)))
    (deftype object-i-opt (union object (tag inaccessible none)))
    (deftype object-u (union object (tag uninitialised)))
    
    (deftype boolean-opt (union boolean (tag none)))
    (deftype integer-opt (union integer (tag none)))
    
    
    (%heading (3 :semantics) "Undefined")
    (deftag undefined)
    (deftype undefined (tag undefined))
    
    
    (%heading (3 :semantics) "Null")
    (deftag null)
    (deftype null (tag null))
    
    
    (%heading (3 :semantics) "Strings")
    (deftype string-opt (union string (tag none)))
    
    
    (%heading (3 :semantics) "Namespaces")
    (defrecord namespace
      (name string))
    
    (define public-namespace namespace (new namespace "public"))
    (define enumerable-namespace namespace (new namespace "enumerable"))
    
    
    (%heading (4 :semantics) "Qualified Names")
    (deftuple qualified-name (namespace namespace) (id string))
    (deftype qualified-name-opt (union qualified-name (tag none)))
    
    (deftype multiname (list-set qualified-name))
    
    
    (%heading (3 :semantics) "Attributes")
    (deftag static)
    (deftag constructor)
    (deftag virtual)
    (deftag final)
    (deftype member-modifier (tag none static constructor virtual final))
    
    (deftype override-modifier (tag none true false undefined))
    
    (deftuple compound-attribute
      (namespaces (list-set namespace))
      (explicit boolean)
      (dynamic boolean)
      (member-mod member-modifier)
      (override-mod override-modifier)
      (prototype boolean)
      (unused boolean))
    
    (deftype attribute (union boolean namespace compound-attribute))
    (deftype attribute-opt-not-false (union (tag none true) namespace compound-attribute))
    
    
    (%heading (3 :semantics) "Classes")
    (defrecord class
      (static-read-bindings (list-set static-binding) :var)
      (static-write-bindings (list-set static-binding) :var)
      (instance-read-bindings (list-set instance-binding) :var)
      (instance-write-bindings (list-set instance-binding) :var)
      (instance-init-order (vector instance-variable))
      (complete boolean :var)
      (super class-opt)
      (prototype object)
      (private-namespace namespace)
      (dynamic boolean)
      (allow-null boolean)
      (final boolean)
      (call (-> (object argument-list phase) object))
      (construct (-> (argument-list phase) object)))
    (deftype class-opt (union class (tag none)))
    
    (%text :comment "Return an ordered list of class " (:local c) :apostrophe "s ancestors, including " (:local c) " itself.")
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
    
    
    (%heading (3 :semantics) "Method Closures")
    (deftuple method-closure
      (this object)
      (method instance-method))
    
    
    (%heading (3 :semantics) "Prototype Instances")
    (defrecord prototype
      (parent prototype-opt)
      (dynamic-properties (list-set dynamic-property) :var))
    (deftype prototype-opt (union prototype (tag none)))
    
    (defrecord dynamic-property 
      (name string)
      (value object :var))
    
    
    (%heading (3 :semantics) "Class Instances")
    (deftype instance (union non-alias-instance alias-instance))
    (deftype non-alias-instance (union fixed-instance dynamic-instance))
    
    (defrecord fixed-instance
      (type class)
      (call (-> (object argument-list environment phase) object))
      (construct (-> (argument-list environment phase) object))
      (env environment)
      (typeof-string string)
      (slots (list-set slot) :var))
    
    (defrecord dynamic-instance
      (type class)
      (call (-> (object argument-list environment phase) object))
      (construct (-> (argument-list environment phase) object))
      (env environment)
      (typeof-string string)
      (slots (list-set slot) :var)
      (dynamic-properties (list-set dynamic-property) :var))
    
    (defrecord alias-instance
      (original non-alias-instance)
      (env environment))
    
    (defrecord open-instance
      (instantiate (-> (environment) non-alias-instance))
      (cache (union non-alias-instance (tag none)) :var))
    
    
    (%heading (4 :semantics) "Slots")
    (defrecord slot
      (id instance-variable)
      (value object-u :var))
    
    
    (%heading (3 :semantics) "Packages")
    (defrecord package
      (static-read-bindings (list-set static-binding) :var)
      (static-write-bindings (list-set static-binding) :var)
      (internal-namespace namespace))
    
    
    (%heading (3 :semantics) "Global Objects")
    (defrecord global
      (static-read-bindings (list-set static-binding) :var)
      (static-write-bindings (list-set static-binding) :var)
      (internal-namespace namespace)
      (dynamic-properties (list-set dynamic-property) :var))
    
    
    (%heading (2 :semantics) "Objects with Limits")
    (%text :comment (:label limited-instance instance) " must be an instance of " (:label limited-instance limit) " or one of "
           (:label limited-instance limit) :apostrophe "s descendants.")
    (deftuple limited-instance
      (instance instance)
      (limit class))
    
    (deftype obj-optional-limit (union object limited-instance))
    
    
    (%heading (2 :semantics) "References")
    (deftuple lexical-reference
      (env environment)
      (variable-multiname multiname)
      (strict boolean))
    
    (deftuple dot-reference
      (base obj-optional-limit)
      (property-multiname multiname))
    
    (deftuple bracket-reference
      (base obj-optional-limit)
      (args argument-list))
    
    (deftype reference (union lexical-reference dot-reference bracket-reference))
    (deftype obj-or-ref (union object reference))
    
    
    (%heading (2 :semantics) "Function Support")
    (deftag normal)
    (deftag get)
    (deftag set)
    (deftype function-kind (tag normal get set))
    
    (deftuple signature
      (positional (vector parameter))
      (optional-positional (vector parameter))
      (optional-named (list-set named-parameter))
      (rest (union parameter (tag none)))
      (rest-allows-names boolean)
      (return-type class))
    
    
    (deftuple parameter
      (local-name qualified-name-opt)
      (type class))
    
    (deftuple named-parameter
      (local-name qualified-name-opt)
      (type class)
      (name string))
    
    
    (%heading (2 :semantics) "Argument Lists")
    (deftuple named-argument (name string) (value object))
    
    (deftuple argument-list
      (positional (vector object))
      (named (list-set named-argument)))
    
    
    (%heading (2 :semantics) "Modes of expression evaluation")
    (deftag compile)
    (deftag run)
    (deftype phase (tag compile run))
    
    
    (%heading (2 :semantics) "Contexts")
    (deftuple context
      (strict boolean)
      (open-namespaces (list-set namespace)))
    
    (define initial-context context
      (new context false (list-set public-namespace)))
    
    
    (%heading (2 :semantics) "Labels")
    (deftag default)
    (deftype label (union string (tag default)))
    
    (deftuple jump-targets
      (break-targets (list-set label))
      (continue-targets (list-set label)))
    
    
    (%heading (2 :semantics) "Environments")
    (deftag singular)
    (deftag plural)
    (deftype plurality (tag singular plural))
    
    (%text :comment "An " (:type environment) " is a list of two or more frames. Each frame corresponds to a scope. "
           "More specific frames are listed first" :m-dash "each frame" :apostrophe "s scope is directly contained in the following frame"
           :apostrophe "s scope. The last frame is always the " (:type system-frame)
           ". The next-to-last frame is always a " (:type package) " or " (:type global) " frame.")
    (deftype environment (vector frame))
    (deftype environment-i (union environment (tag inaccessible)))
    (deftype frame (union system-frame global package parameter-frame class block-frame))
    
    (defrecord system-frame
      (static-read-bindings (list-set static-binding) :var)
      (static-write-bindings (list-set static-binding) :var))
    
    (defrecord parameter-frame
      (static-read-bindings (list-set static-binding) :var)
      (static-write-bindings (list-set static-binding) :var)
      (plurality plurality)
      (this object-i-opt)
      (prototype boolean)
      (signature signature :opt-const))
    
    (defrecord block-frame
      (static-read-bindings (list-set static-binding) :var)
      (static-write-bindings (list-set static-binding) :var)
      (plurality plurality))
    
    (define initial-environment environment (vector-of frame (new system-frame (list-set-of static-binding) (list-set-of static-binding))))
    
    
    (%heading (3 :semantics) "Members")
    (deftuple static-binding 
      (qname qualified-name)
      (content static-member)
      (explicit boolean))
    
    (deftuple instance-binding 
      (qname qualified-name)
      (content instance-member))
    
    (deftag forbidden)
    (deftype static-member (union (tag forbidden) variable hoisted-var constructor-method getter setter))
    (deftype static-member-opt (union static-member (tag none)))
    
    (deftype variable-type (union class (tag inaccessible) (-> () class)))
    (deftype variable-value (union object (tag inaccessible uninitialised) open-instance (-> () object)))
    (defrecord variable
      (type variable-type :var)
      (value variable-value :var)
      (immutable boolean))
    
    (defrecord hoisted-var
      (value (union object open-instance) :var)
      (has-function-initialiser boolean :var))
    
    (defrecord constructor-method
      (code instance))  ;Constructor code
    
    (defrecord getter
      (type class)
      (call (-> (environment phase) object))
      (env environment-i))
    
    (defrecord setter
      (type class)
      (call (-> (object environment phase) void))
      (env environment-i))
    
    
    (deftype instance-member (union instance-variable instance-method instance-getter instance-setter))
    (deftype instance-member-opt (union instance-member (tag none)))
    
    (defrecord instance-variable
      (type class :opt-const)
      (eval-initial-value (-> () object-opt))
      (immutable boolean)
      (final boolean))
    
    (defrecord instance-method
      (code instance)  ;Method code
      (signature signature)
      (final boolean))
    
    (defrecord instance-getter
      (type class :opt-const)
      (call (-> (object environment phase) object))
      (env environment)
      (final boolean))
    
    (defrecord instance-setter
      (type class :opt-const)
      (call (-> (object object environment phase) void))
      (env environment)
      (final boolean))
    
    
    (%heading (1 :semantics) "Data Operations")
    (%heading (2 :semantics) "Numeric Utilities")
    
    (%text :comment (:global-call unsigned-wrap32 i) " returns " (:local i) " converted to a value between 0 and 2" (:superscript "32") :minus
           "1 inclusive, wrapping around modulo 2" (:superscript "32") " if necessary.")
    (define (unsigned-wrap32 (i integer)) (integer-range 0 (- (expt 2 32) 1))
      (return (bitwise-and i (hex #xFFFFFFFF))))
    
    (%text :comment (:global-call signed-wrap32 i) " returns " (:local i) " converted to a value between " :minus "2" (:superscript "31")
           " and 2" (:superscript "31") :minus "1 inclusive, wrapping around modulo 2" (:superscript "32") " if necessary.")
    (define (signed-wrap32 (i integer)) (integer-range (neg (expt 2 31)) (- (expt 2 31) 1))
      (var j integer (bitwise-and i (hex #xFFFFFFFF)))
      (when (>= j (expt 2 31))
        (<- j (- j (expt 2 32))))
      (return j))
    
    (%text :comment (:global-call unsigned-wrap64 i) " returns " (:local i) " converted to a value between 0 and 2" (:superscript "64") :minus
           "1 inclusive, wrapping around modulo 2" (:superscript "64") " if necessary.")
    (define (unsigned-wrap64 (i integer)) (integer-range 0 (- (expt 2 64) 1))
      (return (bitwise-and i (hex #xFFFFFFFFFFFFFFFF))))
    
    (%text :comment (:global-call signed-wrap64 i) " returns " (:local i) " converted to a value between " :minus "2" (:superscript "63")
           " and 2" (:superscript "63") :minus "1 inclusive, wrapping around modulo 2" (:superscript "64") " if necessary.")
    (define (signed-wrap64 (i integer)) (integer-range (neg (expt 2 63)) (- (expt 2 63) 1))
      (var j integer (bitwise-and i (hex #xFFFFFFFFFFFFFFFF)))
      (when (>= j (expt 2 63))
        (<- j (- j (expt 2 64))))
      (return j))
    
    (define (truncate-to-integer (x general-number)) integer
      (case x
        (:select (tag +infinity32 +infinity64 -infinity32 -infinity64 nan32 nan64) (return 0))
        (:narrow finite-float32 (return (truncate-finite-float32 x)))
        (:narrow finite-float64 (return (truncate-finite-float64 x)))
        (:narrow (union long u-long) (return (& value x)))))
    
    (define (check-integer (x general-number)) integer-opt
      (case x
        (:select (tag nan32 nan64 +infinity32 +infinity64 -infinity32 -infinity64) (return none))
        (:select (tag +zero32 +zero64 -zero32 -zero64) (return 0))
        (:narrow (union long u-long) (return (& value x)))
        (:narrow (union nonzero-finite-float32 nonzero-finite-float64)
          (const r rational (& value x))
          (rwhen (not-in r integer :narrow-false)
            (return none))
          (return r))))
    
    (define (integer-to-long (i integer)) general-number
      (cond
       ((cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 63) 1))
        (return (new long i)))
       ((cascade integer (expt 2 63) <= i <= (- (expt 2 64) 1))
        (return (new u-long i)))
       (nil
        (return (real-to-float64 i)))))
    
    (define (integer-to-u-long (i integer)) general-number
      (cond
       ((cascade integer 0 <= i <= (- (expt 2 64) 1))
        (return (new u-long i)))
       ((cascade integer (neg (expt 2 63)) <= i <= -1)
        (return (new long i)))
       (nil
        (return (real-to-float64 i)))))
    
    (define (rational-to-long (q rational)) general-number
      (cond
       ((in q integer :narrow-true) (return (integer-to-long q)))
       ((<= (rat-abs q) (expt 2 53) rational) (return (real-to-float64 q)))
       ((or (< q (rat- (neg (expt 2 63)) (rat/ 1 2)) rational) (>= q (rat- (expt 2 64) (rat/ 1 2)) rational)) (return (real-to-float64 q)))
       (nil
        (/* (:def-const i integer) "Let " (:local i) " be the integer closest to " (:local q)
            ". If " (:local q) " is halfway between two integers, pick " (:local i) " so that it is even.")
        (var i integer (floor q))
        (var frac rational (rat- q i))
        (when (or (> frac (rat/ 1 2) rational) (and (= frac (rat/ 1 2) rational) (= (bitwise-and i 1) 1)))
          (<- i (+ i 1)))
        (*/)
        (assert (cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 64) 1)) "Note that " (:assertion))
        (if (< i (expt 2 63))
          (return (new long i))
          (return (new u-long i))))))
    
    (define (rational-to-u-long (q rational)) general-number
      (cond
       ((in q integer :narrow-true) (return (integer-to-u-long q)))
       ((<= (rat-abs q) (expt 2 53) rational) (return (real-to-float64 q)))
       ((or (< q (rat- (neg (expt 2 63)) (rat/ 1 2)) rational) (>= q (rat- (expt 2 64) (rat/ 1 2)) rational)) (return (real-to-float64 q)))
       (nil
        (/* (:def-const i integer) "Let " (:local i) " be the integer closest to " (:local q)
            ". If " (:local q) " is halfway between two integers, pick " (:local i) " so that it is even.")
        (var i integer (floor q))
        (var frac rational (rat- q i))
        (when (or (> frac (rat/ 1 2) rational) (and (= frac (rat/ 1 2) rational) (= (bitwise-and i 1) 1)))
          (<- i (+ i 1)))
        (*/)
        (assert (cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 64) 1)) "Note that " (:assertion))
        (if (>= i 0)
          (return (new u-long i))
          (return (new long i))))))
    
    (define (to-rational (x finite-general-number)) rational
      (case x
        (:select (tag +zero32 +zero64 -zero32 -zero64) (return 0))
        (:narrow (union nonzero-finite-float32 nonzero-finite-float64 long u-long) (return (& value x)))))
    
    (define (to-float64 (x general-number)) float64
      (case x
        (:narrow (union long u-long) (return (real-to-float64 (& value x))))
        (:narrow float32 (return (float32-to-float64 x)))
        (:narrow float64 (return x))))
    
    
    (deftag less)
    (deftag equal)
    (deftag greater)
    (deftag unordered)
    (deftype order (tag less equal greater unordered))
    
    (define (general-number-compare (x general-number) (y general-number)) order
      (cond
       ((or (in x (tag nan32 nan64) :narrow-false) (in y (tag nan32 nan64) :narrow-false))
        (return unordered))
       ((and (in x (tag +infinity32 +infinity64)) (in y (tag +infinity32 +infinity64)))
        (return equal))
       ((and (in x (tag -infinity32 -infinity64)) (in y (tag -infinity32 -infinity64)))
        (return equal))
       ((or (in x (tag +infinity32 +infinity64) :narrow-false) (in y (tag -infinity32 -infinity64) :narrow-false))
        (return greater))
       ((or (in x (tag -infinity32 -infinity64) :narrow-false) (in y (tag +infinity32 +infinity64) :narrow-false))
        (return less))
       (nil
        (const xr rational (to-rational x))
        (const yr rational (to-rational y))
        (cond
         ((< xr yr rational) (return less))
         ((> xr yr rational) (return greater))
         (nil (return equal))))))
    
    
    (%heading (2 :semantics) "Object Utilities")
    (define (resolve-alias (o instance)) non-alias-instance
      (case o
        (:narrow non-alias-instance (return o))
        (:narrow alias-instance (return (& original o)))))
    
    (%heading (3 :semantics) (:global object-type nil))
    (%text :comment (:global-call object-type o) " returns an " (:type object) " " (:local o) :apostrophe "s most specific type.")
    (define (object-type (o object)) class
      (case o
        (:select undefined (return undefined-class))
        (:select null (return null-class))
        (:select boolean (return boolean-class))
        (:select long (return long-class))
        (:select u-long (return u-long-class))
        (:select float32 (return float-class))
        (:select float64 (return number-class))
        (:select character (return character-class))
        (:select string (return string-class))
        (:select namespace (return namespace-class))
        (:select compound-attribute (return attribute-class))
        (:select class (return class-class))
        (:select method-closure (return function-class))
        (:select prototype (return prototype-class))
        (:narrow instance (return (& type (resolve-alias o))))
        (:select (union package global) (return package-class))))
    
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
                  (and (= o null object) (& allow-null c)))))
    
    
    (%heading (3 :semantics) (:global to-boolean nil))
    (%text :comment (:global-call to-boolean o phase) " coerces an object " (:local o) " to a Boolean. If "
           (:local phase) " is " (:tag compile) ", only compile-time conversions are permitted.")
    (define (to-boolean (o object) (phase phase :unused)) boolean
      (case o
        (:select (union undefined null) (return false))
        (:narrow boolean (return o))
        (:narrow (union long u-long) (return (/= (& value o) 0)))
        (:narrow float32 (return (not-in o (tag +zero32 -zero32 nan32))))
        (:narrow float64 (return (not-in o (tag +zero64 -zero64 nan64))))
        (:narrow string (return (/= o "" string)))
        (:select (union character namespace compound-attribute class method-closure prototype instance package global) (return true))))
    
    
    (%heading (3 :semantics) (:global to-general-number nil))
    (%text :comment (:global-call to-general-number o phase) " coerces an object " (:local o) " to a " (:type general-number) ". If "
           (:local phase) " is " (:tag compile) ", only compile-time conversions are permitted.")
    (define (to-general-number (o object) (phase phase :unused)) general-number
      (case o
        (:select undefined (return nan64))
        (:select (union null (tag false)) (return +zero64))
        (:select (tag true) (return 1.0))
        (:narrow general-number (return o))
        (:select (union character string) (todo))
        (:select (union namespace compound-attribute class method-closure package global) (throw bad-value-error))
        (:select (union prototype instance) (todo))))
    
    
    (%heading (3 :semantics) (:global to-string nil))
    (%text :comment (:global-call to-string o phase) " coerces an object " (:local o) " to a string. If "
           (:local phase) " is " (:tag compile) ", only compile-time conversions are permitted.")
    (define (to-string (o object) (phase phase :unused)) string
      (case o
        (:select undefined (return "undefined"))
        (:select null (return "null"))
        (:select (tag false) (return "false"))
        (:select (tag true) (return "true"))
        (:narrow (union long u-long) (return (integer-to-string (& value o))))
        (:narrow float32 (return (float32-to-string o)))
        (:narrow float64 (return (float64-to-string o)))
        (:narrow character (return (vector o)))
        (:narrow string (return o))
        (:select namespace (todo))
        (:select compound-attribute (todo))
        (:select class (todo))
        (:select method-closure (todo))
        (:select (union prototype instance) (todo))
        (:select (union package global) (todo))))
    
    
    (%text :comment (:global-call integer-to-string i) " converts an integer " (:local i) " to a string of one or more decimal digits. If "
           (:local i) " is negative, the string is preceded by a minus sign.")
    (define (integer-to-string (i integer)) string
      (rwhen (< i 0)
        (return (cons #\- (integer-to-string (neg i)))))
      (const q integer (floor (rat/ i 10)))
      (const r integer (- i (* q 10)))
      (const c character (code-to-character (+ r (character-to-code #\0))))
      (if (= q 0)
        (return (vector c))
        (return (append (integer-to-string q) (vector c)))))
    
    
    (%text :comment (:global-call integer-to-string-with-sign i) " is the same as " (:global-call integer-to-string i)
           " except that the resulting string always begins with a plus or minus sign.")
    (define (integer-to-string-with-sign (i integer)) string
      (if (>= i 0)
        (return (cons #\+ (integer-to-string i)))
        (return (cons #\- (integer-to-string (neg i))))))
    
    
    (%text :comment (:global-call float32-to-string x) " converts a " (:type float32) " " (:local x) " to a string using fixed-point notation if "
           "the absolute value of " (:local x) " is between " (:expr rational (expt 10 -6)) " inclusive and " (:expr rational (expt 10 21)) " exclusive and "
           "exponential notation otherwise. The result has the fewest significant digits possible while still ensuring that converting the string back into a "
           (:type float32) " value would result in the same value " (:local x) " (except that " (:tag -zero32) " would become " (:tag +zero32) ").")
    (define (float32-to-string (x float32)) string
      (case x
        (:select (tag nan32) (return "NaN"))
        (:select (tag +zero32 -zero32) (return "0"))
        (:select (tag +infinity32) (return "Infinity"))
        (:select (tag -infinity32) (return "-Infinity"))
        (:narrow nonzero-finite-float32
          (const r rational (& value x))
          (cond
           ((< r 0 rational) (return (append "-" (float32-to-string (float32-negate x)))))
           (nil
            (/* (:def-const n integer) (:def-const k integer) (:def-const s integer)
                "Let " (:local n) ", " (:local k) ", and " (:local s) " be integers such that "
                (:expr boolean (>= k 1)) ", " (:expr boolean (cascade rational (expt 10 (- k 1)) <= s <= (expt 10 k))) ", "
                (:expr boolean (= (real-to-float32 (rat* s (expt 10 (- n k)))) x float32)) ", and " (:local k) " is as small as possible. "
                "Note that " (:local k) " is the number of digits in the decimal representation of " (:local s) ", that " (:local s)
                " is not divisible by 10, and that the least significant digit of " (:local s)
                " is not necessarily uniquely determined by these criteria.")
            (const n integer (bottom))
            (const k integer (bottom))
            (const s integer (bottom))
            (*/)
            (// "When there are multiple possibilities for " (:local s) " according to the rules above, "
                "implementations are encouraged but not required to select the one according to the following rules: "
                "Select the value of " (:local s) " for which " (:expr rational (rat* s (expt 10 (- n k)))) " is closest in value to " (:local r)
                "; if there are two such possible values of " (:local s) ", choose the one that is even.")
            (const digits string (integer-to-string s))
            (cond
             ((cascade integer k <= n <= 21)
              (return (append digits (repeat character #\0 (- n k)))))
             ((cascade integer 0 < n <= 21)
              (return (append (subseq digits 0 (- n 1)) "." (subseq digits n))))
             ((cascade integer -6 < n <= 0)
              (return (append "0." (repeat character #\0 (neg n)) digits)))
             (nil
              (var mantissa string)
              (if (= k 1)
                (<- mantissa digits)
                (<- mantissa (append (subseq digits 0 0) "." (subseq digits 1))))
              (return (append mantissa "e" (integer-to-string-with-sign (- n 1)))))))))))
    (defprimitive float32-to-string (lambda (x) (float32-to-string x)))
    
    
    (%text :comment (:global-call float64-to-string x) " converts a " (:type float64) " " (:local x) " to a string using fixed-point notation if "
           "the absolute value of " (:local x) " is between " (:expr rational (expt 10 -6)) " inclusive and " (:expr rational (expt 10 21)) " exclusive and "
           "exponential notation otherwise. The result has the fewest significant digits possible while still ensuring that converting the string back into a "
           (:type float64) " value would result in the same value " (:local x) " (except that " (:tag -zero64) " would become " (:tag +zero64) ").")
    (define (float64-to-string (x float64)) string
      (case x
        (:select (tag nan64) (return "NaN"))
        (:select (tag +zero64 -zero64) (return "0"))
        (:select (tag +infinity64) (return "Infinity"))
        (:select (tag -infinity64) (return "-Infinity"))
        (:narrow nonzero-finite-float64
          (const r rational (& value x))
          (cond
           ((< r 0 rational) (return (append "-" (float64-to-string (float64-negate x)))))
           (nil
            (/* (:def-const n integer) (:def-const k integer) (:def-const s integer)
                "Let " (:local n) ", " (:local k) ", and " (:local s) " be integers such that "
                (:expr boolean (>= k 1)) ", " (:expr boolean (cascade rational (expt 10 (- k 1)) <= s <= (expt 10 k))) ", "
                (:expr boolean (= (real-to-float64 (rat* s (expt 10 (- n k)))) x float64)) ", and " (:local k) " is as small as possible. "
                "Note that " (:local k) " is the number of digits in the decimal representation of " (:local s) ", that " (:local s)
                " is not divisible by 10, and that the least significant digit of " (:local s)
                " is not necessarily uniquely determined by these criteria.")
            (const n integer (bottom))
            (const k integer (bottom))
            (const s integer (bottom))
            (*/)
            (// "When there are multiple possibilities for " (:local s) " according to the rules above, "
                "implementations are encouraged but not required to select the one according to the following rules: "
                "Select the value of " (:local s) " for which " (:expr rational (rat* s (expt 10 (- n k)))) " is closest in value to " (:local r)
                "; if there are two such possible values of " (:local s) ", choose the one that is even.")
            (const digits string (integer-to-string s))
            (cond
             ((cascade integer k <= n <= 21)
              (return (append digits (repeat character #\0 (- n k)))))
             ((cascade integer 0 < n <= 21)
              (return (append (subseq digits 0 (- n 1)) "." (subseq digits n))))
             ((cascade integer -6 < n <= 0)
              (return (append "0." (repeat character #\0 (neg n)) digits)))
             (nil
              (var mantissa string)
              (if (= k 1)
                (<- mantissa digits)
                (<- mantissa (append (subseq digits 0 0) "." (subseq digits 1))))
              (return (append mantissa "e" (integer-to-string-with-sign (- n 1)))))))))))
    (defprimitive float64-to-string (lambda (x) (float64-to-string x)))
    
    
    (%heading (3 :semantics) (:global to-primitive nil))
    (define (to-primitive (o object) (hint object :unused) (phase phase)) primitive-object
      (case o
        (:narrow primitive-object (return o))
        (:select (union namespace compound-attribute class method-closure prototype instance package global) (return (to-string o phase)))))
    
    
    (%heading (3 :semantics) (:global assignment-conversion nil))
    (define (assignment-conversion (o object) (type class)) object
      (rwhen (relaxed-has-type o type)
        (return o))
      (todo))
    
    
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
          (return (new compound-attribute (list-set a b) false false none none false false)))
         (nil
          (return (set-field b namespaces (set+ (& namespaces b) (list-set a)))))))
       ((in b namespace :narrow-both)
        (return (set-field a namespaces (set+ (& namespaces a) (list-set b)))))
       (nil
        (// "Both " (:local a) " and " (:local b) " are compound attributes. Ensure that they have no conflicting contents.")
        (if (or (and (not-in (& member-mod a) (tag none)) (not-in (& member-mod b) (tag none)) (/= (& member-mod a) (& member-mod b) member-modifier))
                (and (not-in (& override-mod a) (tag none)) (not-in (& override-mod b) (tag none)) (/= (& override-mod a) (& override-mod b) override-modifier)))
          (throw bad-value-error)
          (return (new compound-attribute
                       (set+ (& namespaces a) (& namespaces b))
                       (or (& explicit a) (& explicit b))
                       (or (& dynamic a) (& dynamic b))
                       (if (not-in (& member-mod a) (tag none)) (& member-mod a) (& member-mod b))
                       (if (not-in (& override-mod a) (tag none)) (& override-mod a) (& override-mod b))
                       (or (& prototype a) (& prototype b))
                       (or (& unused a) (& unused b))))))))
    
    
    (%text :comment (:global-call to-compound-attribute a) " returns " (:local a) " converted to a " (:type compound-attribute)
           " even if it was a simple namespace, " (:tag true) ", or " (:tag none) ".")
    (define (to-compound-attribute (a attribute-opt-not-false)) compound-attribute
      (case a
        (:select (tag none true) (return (new compound-attribute (list-set-of namespace) false false none none false false)))
        (:narrow namespace (return (new compound-attribute (list-set a) false false none none false false)))
        (:narrow compound-attribute (return a))))
    
    
    (%heading (2 :semantics) "References")
    (%text :comment "If " (:local r) " is an " (:type object) ", " (:global-call read-reference r phase) " returns it unchanged.  If "
           (:local r) " is a " (:type reference) ", this function reads " (:local r) " and returns the result. If "
           (:local phase) " is " (:tag compile) ", only compile-time expressions can be evaluated in the process of reading " (:local r) ".")
    (define (read-reference (r obj-or-ref) (phase phase)) object
      (case r
        (:narrow object (return r))
        (:narrow lexical-reference (return (lexical-read (& env r) (& variable-multiname r) phase)))
        (:narrow dot-reference
          (const result object-opt (read-property (& base r) (& property-multiname r) property-lookup phase))
          (if (not-in result (tag none) :narrow-true)
            (return result)
            (throw property-access-error)))
        (:narrow bracket-reference (return (bracket-read (& base r) (& args r) phase)))))
    
    (define (bracket-read (a obj-optional-limit) (args argument-list) (phase phase)) object
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0) phase))
      (const result object-opt (read-property a (list-set (new qualified-name public-namespace name)) property-lookup phase))
      (if (not-in result (tag none) :narrow-true)
        (return result)
        (throw property-access-error)))
    
    
    (%text :comment "If " (:local r) " is a reference, " (:global-call write-reference r new-value) " writes " (:local new-value) 
           " into " (:local r) ". An error occurs if " (:local r) " is not a reference. "
           (:local r) :apostrophe "s limit, if any, is ignored. "
           (:global write-reference) " is never called from a compile-time expression.")
    (define (write-reference (r obj-or-ref) (new-value object) (phase (tag run))) void
      (var result (tag none ok))
      (case r
        (:select object (throw reference-error))
        (:narrow lexical-reference
          (lexical-write (& env r) (& variable-multiname r) new-value (not (& strict r)) phase)
          (return))
        (:narrow dot-reference (<- result (write-property (& base r) (& property-multiname r) property-lookup true new-value phase)))
        (:narrow bracket-reference (<- result (bracket-write (& base r) (& args r) new-value phase))))
      (rwhen (in result (tag none))
        (throw property-access-error)))
    
    (define (bracket-write (a obj-optional-limit) (args argument-list) (new-value object) (phase phase)) (tag none ok)
      (rwhen (in phase (tag compile) :narrow-false)
        (throw compile-expression-error))
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0) phase))
      (return (write-property a (list-set (new qualified-name public-namespace name)) property-lookup true new-value phase)))
    
    
    (%text :comment "If " (:local r) " is a " (:type reference) ", " (:global-call delete-reference r) " deletes it. If "
           (:local r) " is an " (:type object) ", this function signals an error in strict mode or returns " (:tag true) " in non-strict mode. "
           (:global delete-reference) " is never called from a compile-time expression.")
    (define (delete-reference (r obj-or-ref) (strict boolean) (phase (tag run))) boolean
      (var result boolean-opt)
      (case r
        (:select object
          (if strict
            (throw reference-error)
            (return true)))
        (:narrow lexical-reference (return (lexical-delete (& env r) (& variable-multiname r) phase)))
        (:narrow dot-reference (<- result (delete-property (& base r) (& property-multiname r) property-lookup phase)))
        (:narrow bracket-reference (<- result (bracket-delete (& base r) (& args r) phase))))
      (if (not-in result (tag none) :narrow-true)
        (return result)
        (return true)))
    
    (define (bracket-delete (a obj-optional-limit) (args argument-list) (phase phase)) boolean-opt
      (rwhen (in phase (tag compile) :narrow-false)
        (throw compile-expression-error))
      (rwhen (or (/= (length (& positional args)) 1) (nonempty (& named args)))
        (throw argument-mismatch-error))
      (const name string (to-string (nth (& positional args) 0) phase))
      (return (delete-property a (list-set (new qualified-name public-namespace name)) property-lookup phase)))
    
    
    (%heading (2 :semantics) "Slots")
    
    (define (find-slot (o object) (id instance-variable)) slot
      (assert (in o instance :narrow-true) (:local o) " must be an " (:type instance) ";")
      (const matching-slots (list-set slot)
        (map (& slots (resolve-alias o)) s s (= (& id s) id instance-variable)))
      (return (unique-elt-of matching-slots)))
    
    
    (%heading (2 :semantics) "Environments")
    
    (%text :comment "If " (:local env) " is from within a class" :apostrophe "s body, "
           (:global-call get-enclosing-class env) " returns the innermost such class; otherwise, it returns " (:tag none) ".")
    (define (get-enclosing-class (env environment)) class-opt
      (reserve c)
      (rwhen (some env c (in c class :narrow-true) :define-true)
        (// "Let " (:local c) " be the first element of " (:local env) " that is a " (:type class) ".")
        (return c))
      (return none))
    
    
    (%text :comment (:global-call get-regional-environment env) " returns all frames in " (:local env) " up to and including the first regional frame. "
           "A regional frame is either any frame other than a local block frame or a local block frame whose immediate enclosing frame is a class.")
    (define (get-regional-environment (env environment)) (vector frame)
      (var i integer 0)
      (while (in (nth env i) block-frame)
        (<- i (+ i 1)))
      (when (and (/= i 0) (in (nth env i) class))
        (<- i (- i 1)))
      (return (subseq env 0 i)))
    
    
    (%text :comment (:global-call get-regional-frame env) " returns the most specific regional frame in " (:local env) ".")
    (define (get-regional-frame (env environment)) frame
      (const regional-env (vector frame) (get-regional-environment env))
      (return (nth regional-env (- (length regional-env) 1))))
    
    
    (define (get-package-or-global-frame (env environment)) (union package global)
      (const g frame (nth env (- (length env) 2)))
      (assert (in g (union package global) :narrow-true) "The penultimate frame " (:local g) " is always a " (:type package) " or " (:type global) " frame.")
      (return g))
    
    
    (%heading (3 :semantics) "Access Utilities")
    (deftag read)
    (deftag write)
    (deftag read-write)
    (deftype access (tag read write read-write))
    
    
    (%text :comment (:global-call static-bindings-with-access f access) " returns the set of static bindings in frame " (:local f)
           " which are used for reading, writing, or either, as selected by " (:local access) ".")
    (define (static-bindings-with-access (f frame) (access access)) (list-set static-binding)
      (case access
        (:select (tag read) (return (& static-read-bindings f)))
        (:select (tag write) (return (& static-write-bindings f)))
        (:select (tag read-write) (return (set+ (& static-read-bindings f) (& static-write-bindings f))))))
    
    
    (%text :comment (:global-call instance-bindings-with-access c access) " returns the set of instance bindings in class " (:local c)
           " which are used for reading, writing, or either, as selected by " (:local access) ".")
    (define (instance-bindings-with-access (c class) (access access)) (list-set instance-binding)
      (case access
        (:select (tag read) (return (& instance-read-bindings c)))
        (:select (tag write) (return (& instance-write-bindings c)))
        (:select (tag read-write) (return (set+ (& instance-read-bindings c) (& instance-write-bindings c))))))
    
    
    (%text :comment (:global-call add-static-bindings f access new-bindings) " adds " (:local new-bindings)
           " to the set of readable, writable, or both (as selected by " (:local access) ") static bindings in frame " (:local f) ".")
    (define (add-static-bindings (f frame) (access access) (new-bindings (list-set static-binding))) void
      (when (in access (tag read read-write))
        (&= static-read-bindings f (set+ (& static-read-bindings f) new-bindings)))
      (when (in access (tag write read-write))
        (&= static-write-bindings f (set+ (& static-write-bindings f) new-bindings))))
    
    
    (%heading (3 :semantics) "Adding Static Definitions")
    (define (define-static-member (env environment) (id string) (namespaces (list-set namespace)) (override-mod override-modifier) (explicit boolean)
              (access access) (m static-member))
            multiname
      (const local-frame frame (nth env 0))
      (rwhen (or (not-in override-mod (tag none)) (and explicit (not-in local-frame package)))
        (throw definition-error))
      (var namespaces2 (list-set namespace) namespaces)
      (when (empty namespaces2)
        (<- namespaces2 (list-set public-namespace)))
      (const multiname multiname (map namespaces2 ns (new qualified-name ns id)))
      (const regional-env (vector frame) (get-regional-environment env))
      (const regional-frame frame (nth regional-env (- (length regional-env) 1)))
      (rwhen (some (static-bindings-with-access local-frame access) b (set-in (& qname b) multiname))
        (throw definition-error))
      (for-each (subseq regional-env 1) frame
        (rwhen (some (static-bindings-with-access frame access) b (and (set-in (& qname b) multiname) (not-in (& content b) (tag forbidden))))
          (throw definition-error)))
      (rwhen (and (in regional-frame global :narrow-true)
                  (some (& dynamic-properties regional-frame) dp (set-in (new qualified-name public-namespace (& name dp)) multiname)))
        (throw definition-error))
      (const new-bindings (list-set static-binding) (map multiname qname (new static-binding qname m explicit)))
      (add-static-bindings local-frame access new-bindings)
      (// "Mark the bindings of " (:local multiname) " as " (:tag forbidden)
          " in all non-innermost frames in the current region if they haven" :apostrophe "t been marked as such already.")
      (const new-forbidden-bindings (list-set static-binding) (map multiname qname (new static-binding qname forbidden true)))
      (for-each (subseq regional-env 1) frame
        (add-static-bindings frame access new-forbidden-bindings))
      (return multiname))
      
    
    (define (define-hoisted-var (env environment) (id string)) void
      (const qname qualified-name (new qualified-name public-namespace id))
      (const regional-env (vector frame) (get-regional-environment env))
      (var regional-frame frame (nth regional-env (- (length regional-env) 1)))
      (assert (in regional-frame (union global parameter-frame) :narrow-true)
              (:local env) " is either the " (:type global) " frame or a " (:type parameter-frame)
              " because hoisting only occurs into global or function scope.")
      (var existing-bindings (list-set static-binding) (map (static-bindings-with-access regional-frame read-write) b b
                                                            (= (& qname b) qname qualified-name)))
      (when (empty existing-bindings)
        (case regional-frame
          (:narrow global
            (rwhen (some (& dynamic-properties regional-frame) dp (= (& name dp) id string))
              (throw definition-error)))
          (:select parameter-frame
            (when (>= (length regional-env) 2)
              (<- regional-frame (nth regional-env (- (length regional-env) 2)) :end-narrow)
              (<- existing-bindings (map (static-bindings-with-access regional-frame read-write) b b
                                         (= (& qname b) qname qualified-name)))))))
      (cond
       ((empty existing-bindings)
        (const v hoisted-var (new hoisted-var undefined false))
        (add-static-bindings regional-frame read-write (list-set (new static-binding qname v false))))
       ((some existing-bindings b (not-in (& content b) hoisted-var))
        (throw definition-error))
       (nil
        (// "A hoisted binding of the same " (:character-literal "var") " already exists, so there is no need to create another one."))))
    
    
    (%heading (3 :semantics) "Adding Instance Definitions")
    (deftuple override-status-pair
      (read-status override-status)
      (write-status override-status))
    
    (deftag potential-conflict)
    (deftype overridden-member (union instance-member (tag none potential-conflict)))
    (deftuple override-status
      (overridden-member overridden-member)
      (multiname multiname))
    
    
    (define (search-for-overrides (c class) (id string) (namespaces (list-set namespace)) (access (tag read write)))
            override-status
      (var multiname multiname (list-set-of qualified-name))
      (var overridden-member instance-member-opt none)
      (const s class-opt (& super c))
      (for-each namespaces ns
        (const qname qualified-name (new qualified-name ns id))
        (const m instance-member-opt (find-instance-member s qname access))
        (when (not-in m (tag none) :narrow-true)
          (<- multiname (set+ multiname (list-set qname)))
          (cond
           ((in overridden-member (tag none) :narrow-false) (<- overridden-member m))
           ((/= overridden-member m instance-member) (throw definition-error)))))
      (return (new override-status overridden-member multiname)))
    
    
    (define (resolve-overrides (c class) (cxt context) (id string) (namespaces (list-set namespace)) (access (tag read write)) (expect-method boolean))
            override-status
      (var os override-status)
      (cond
       ((empty namespaces)
        (<- os (search-for-overrides c id (& open-namespaces cxt) access))
        (when (in (& overridden-member os) (tag none))
          (<- os (new override-status none (list-set (new qualified-name public-namespace id))))))
       (nil
        (const defined-multiname multiname (map namespaces ns (new qualified-name ns id)))
        (const os2 override-status (search-for-overrides c id namespaces access))
        (cond
         ((in (& overridden-member os2) (tag none))
          (const os3 override-status (search-for-overrides c id (set- (& open-namespaces cxt) namespaces) access))
          (if (in (& overridden-member os3) (tag none))
            (<- os (new override-status none defined-multiname))
            (<- os (new override-status potential-conflict defined-multiname))))
         (nil
          (<- os (new override-status (& overridden-member os2) (set+ (& multiname os2) defined-multiname)))))))
      (rwhen (some (instance-bindings-with-access c access) b (set-in (& qname b) (& multiname os)))
        (throw definition-error))
      (if expect-method
        (rwhen (not-in (& overridden-member os) (union (tag none potential-conflict) instance-method))
          (throw definition-error))
        (rwhen (not-in (& overridden-member os) (union (tag none potential-conflict) instance-variable instance-getter instance-setter))
          (throw definition-error)))
      (return os))
    
    
    (define (define-instance-member (c class) (cxt context) (id string) (namespaces (list-set namespace))
              (override-mod override-modifier) (explicit boolean) (access access) (m instance-member))
            override-status-pair
      (rwhen explicit
        (throw definition-error))
      (const expect-method boolean (in m instance-method))
      (const read-status override-status
        (if (in access (tag read read-write))
          (resolve-overrides c cxt id namespaces read expect-method)
          (new override-status none (list-set-of qualified-name))))
      (const write-status override-status
        (if (in access (tag write read-write))
          (resolve-overrides c cxt id namespaces write expect-method)
          (new override-status none (list-set-of qualified-name))))
      (cond
       ((or (in (& overridden-member read-status) instance-member) (in (& overridden-member write-status) instance-member))
        (rwhen (not-in override-mod (tag true undefined))
          (throw definition-error)))
       ((or (in (& overridden-member read-status) (tag potential-conflict)) (in (& overridden-member write-status) (tag potential-conflict)))
        (rwhen (not-in override-mod (tag false undefined))
          (throw definition-error)))
       (nil
        (rwhen (not-in override-mod (tag none false undefined))
          (throw definition-error))))
      (const new-read-bindings (list-set instance-binding) (map (& multiname read-status) qname (new instance-binding qname m)))
      (&= instance-read-bindings c (set+ (& instance-read-bindings c) new-read-bindings))
      (const new-write-bindings (list-set instance-binding) (map (& multiname write-status) qname (new instance-binding qname m)))
      (&= instance-write-bindings c (set+ (& instance-write-bindings c) new-write-bindings))
      (return (new override-status-pair read-status write-status)))
    
    
    (%heading (3 :semantics) "Instantiation")
    
    (define (instantiate-open-instance (oi open-instance) (env environment)) instance
      (const cache (union fixed-instance dynamic-instance (tag none)) (& cache oi))
      (cond
       ((in cache (tag none) :narrow-false)
        (const i non-alias-instance ((& instantiate oi) env))
        (var reuse boolean)
        (/* "At the implementation" :apostrophe "s discretion, either " (:local reuse) :nbsp :assign-10 :nbsp (:tag true) ", or "
            (:local reuse) :nbsp :assign-10 :nbsp (:tag false) ". An implementation may make different choices at different times. "
            "The intent here is to allow implementations the freedom to reuse a closure object rather than create a new closure each "
            "time a particular " (:type open-instance) " is instantiated if the implementation notices that the resulting closures would be "
            "behaviorally indistinguishable from each other.")
        (<- reuse true)
        (*/)
        (when reuse
          (&= cache oi i))
        (return i))
       (nil
        (return (new alias-instance cache env)))))
    
    
    (define (instantiate-member (m static-member) (env environment)) static-member
      (case m
        (:narrow (tag forbidden) (return m))
        (:narrow variable
          (var value variable-value (& value m))
          (when (in value open-instance :narrow-true)
            (<- value (instantiate-open-instance value env) :end-narrow))
          (return (new variable (& type m) value (& immutable m))))
        (:narrow hoisted-var
          (var value (union object open-instance) (& value m))
          (when (in value open-instance :narrow-true)
            (<- value (instantiate-open-instance value env) :end-narrow))
          (return (new hoisted-var value (& has-function-initialiser m))))
        (:narrow constructor-method (return m))
        (:narrow getter
          (case (& env m)
            (:select environment (return m))
            (:select (tag inaccessible) (return (new getter (& type m) (& call m) env)))))
        (:narrow setter
          (case (& env m)
            (:select environment (return m))
            (:select (tag inaccessible) (return (new setter (& type m) (& call m) env)))))))
    
    
    (deftuple member-instantiation
      (plural-member static-member)
      (singular-member static-member))
    
    (define (instantiate-frame (plural-frame (union parameter-frame block-frame)) (singular-frame (union parameter-frame block-frame)) (env environment))
            void
      (const plural-members (list-set static-member)
        (map (set+ (& static-read-bindings plural-frame) (& static-write-bindings plural-frame)) b (& content b)))
      (const member-instantiations (list-set member-instantiation)
        (map plural-members m (new member-instantiation m (instantiate-member m env))))
      (function (instantiate-binding (b static-binding)) static-binding
        (const mi member-instantiation (unique-elt-of member-instantiations mi (= (& plural-member mi) (& content b) static-member)))
        (return (new static-binding (& qname b) (& singular-member mi) (& explicit b))))
      (&= static-read-bindings singular-frame (map (& static-read-bindings plural-frame) b (instantiate-binding b)))
      (&= static-write-bindings singular-frame (map (& static-write-bindings plural-frame) b (instantiate-binding b))))
    
    
    
    (%heading (3 :semantics) "Environmental Lookup")
    
    (%text :comment (:global-call find-this env allow-prototype-this) " returns the value of " (:character-literal "this")
           ". If " (:local allow-prototype-this) " is " (:tag true) ", allow " (:character-literal "this")
           " to be defined by either an instance member of a class or a "
           (:character-literal "prototype") " function. If " (:local allow-prototype-this) " is " (:tag false) ", allow " (:character-literal "this")
           " to be defined only by an instance member of a class.")
    (define (find-this (env environment) (allow-prototype-this boolean)) object-i-opt
      (for-each env frame
        (when (and (in frame parameter-frame :narrow-true) (not-in (& this frame) (tag none)))
          (when (or allow-prototype-this (not (& prototype frame)))
            (return (& this frame)))))
      (return none))
    
    
    (define (lexical-read (env environment) (multiname multiname) (phase phase)) object
      (const kind lookup-kind (new lexical-lookup (find-this env false)))
      (var i integer 0)
      (while (< i (length env))
        (const frame frame (nth env i))
        (const result object-opt (read-property frame multiname kind phase))
        (rwhen (not-in result (tag none) :narrow-true)
          (return result))
        (<- i (+ i 1)))
      (throw reference-error))
    
    
    (define (lexical-write (env environment) (multiname multiname) (new-value object) (create-if-missing boolean) (phase (tag run))) void
      (const kind lookup-kind (new lexical-lookup (find-this env false)))
      (var i integer 0)
      (while (< i (length env))
        (const frame frame (nth env i))
        (const result (tag none ok) (write-property frame multiname kind false new-value phase))
        (rwhen (in result (tag ok))
          (return))
        (<- i (+ i 1)))
      (when create-if-missing
        (const g (union package global) (get-package-or-global-frame env))
        (when (in g global)
          (// "Now try to write the variable into " (:local g) " again, this time allowing new dynamic bindings to be created dynamically.")
          (const result (tag none ok) (write-property g multiname kind true new-value phase))
          (rwhen (in result (tag ok))
            (return))))
      (throw reference-error))
    
    
    (define (lexical-delete (env environment) (multiname multiname) (phase (tag run))) boolean
      (const kind lookup-kind (new lexical-lookup (find-this env false)))
      (var i integer 0)
      (while (< i (length env))
        (const frame frame (nth env i))
        (const result boolean-opt (delete-property frame multiname kind phase))
        (rwhen (not-in result (tag none) :narrow-true)
          (return result))
        (<- i (+ i 1)))
      (return true))
    
    
    
    (%heading (3 :semantics) "Property Lookup")
    
    (deftag property-lookup)
    (deftuple lexical-lookup (this object-i-opt))
    (deftype lookup-kind (union (tag property-lookup) lexical-lookup))
    
    
    (define (select-public-name (multiname multiname)) string-opt
      (reserve qname)
      (rwhen (some multiname qname (= (& namespace qname) public-namespace namespace) :define-true)
        (return (& id qname)))
      (return none))
        
    
    (define (find-flat-member (frame frame) (multiname multiname) (access (tag read write)) (phase phase :unused))
            static-member-opt
      (const matching-bindings (list-set static-binding) (map (static-bindings-with-access frame access) b b (set-in (& qname b) multiname)))
      (when (empty matching-bindings)
        (return none))
      (const matching-members (list-set static-member) (map matching-bindings b (& content b)))
      (// "Note that if the same member was found via several different bindings " (:local b)
          ", then it will appear only once in the set " (:local matching-members) ".")
      (rwhen (> (length matching-members) 1)
        (// "This access is ambiguous because the bindings it found belong to several different members in the same class.")
        (throw property-access-error))
      (return (unique-elt-of matching-members)))
    
    
    (define (find-static-member (c class-opt) (multiname multiname) (access (tag read write)) (phase phase :unused))
            (union (tag none) static-member qualified-name)
      (var s class-opt c)
      (while (not-in s (tag none) :narrow-true)
        (const matching-static-bindings (list-set static-binding) (map (static-bindings-with-access s access) b b (set-in (& qname b) multiname)))
        (// "Note that if the same member was found via several different bindings " (:local b)
            ", then it will appear only once in the set " (:local matching-static-members) ".")
        (const matching-static-members (list-set static-member) (map matching-static-bindings b (& content b)))
        (when (nonempty matching-static-members)
          (cond
           ((= (length matching-static-members) 1)
            (return (unique-elt-of matching-static-members)))
           (nil
            (// "This access is ambiguous because the bindings it found belong to several different static members in the same class.")
            (throw property-access-error))))
        (// "If a static member wasn't found in a class, look for an instance member in that class as well.")
        (const matching-instance-bindings (list-set instance-binding) (map (instance-bindings-with-access s access) b b (set-in (& qname b) multiname)))
        (// "Note that if the same " (:type instance-member) " was found via several different bindings " (:local b)
            ", then it will appear only once in the set " (:local matching-instance-members) ".")
        (const matching-instance-members (list-set instance-member) (map matching-instance-bindings b (& content b)))
        (when (nonempty matching-instance-members)
          (cond
           ((= (length matching-instance-members) 1)
            (// "Return the qualified name of any matching binding.  It doesn" :apostrophe
                "t matter which because they all refer to the same " (:type instance-member)
                ", and if one is overridden by a subclass then all must be overridden in the same way by that subclass.")
            (const b instance-binding (elt-of matching-instance-bindings))
            (return (& qname b)))
           (nil
            (// "This access is ambiguous because the bindings it found belong to several different members in the same class.")
            (throw property-access-error))))
        (<- s (& super s) :end-narrow))
      (return none))
    
    
    (define (resolve-instance-member-name (c class) (multiname multiname) (access (tag read write)) (phase phase :unused))
            qualified-name-opt
      (// "Start from the root class (" (:character-literal "Object") ") and proceed through more specific classes that are ancestors of " (:local c) ".")
      (for-each (ancestors c) s
        (const matching-instance-bindings (list-set instance-binding) (map (instance-bindings-with-access s access) b b (set-in (& qname b) multiname)))
        (// "Note that if the same " (:type instance-member) " was found via several different bindings " (:local b)
            ", then it will appear only once in the set " (:local matching-members) ".")
        (const matching-instance-members (list-set instance-member) (map matching-instance-bindings b (& content b)))
        (when (nonempty matching-instance-members)
          (cond
           ((= (length matching-instance-members) 1)
            (// "Return the qualified name of any matching binding.  It doesn" :apostrophe
                "t matter which because they all refer to the same " (:type instance-member)
                ", and if one is overridden by a subclass then all must be overridden in the same way by that subclass.")
            (const b instance-binding (elt-of matching-instance-bindings))
            (return (& qname b)))
           (nil
            (// "This access is ambiguous because the bindings it found belong to several different members in the same class.")
            (throw property-access-error)))))
      (return none))
    
    
    (define (find-instance-member (c class-opt) (qname qualified-name-opt) (access (tag read write))) instance-member-opt
      (rwhen (in qname (tag none) :narrow-false)
        (return none))
      (var s class-opt c)
      (while (not-in s (tag none) :narrow-true)
        (reserve b)
        (rwhen (some (instance-bindings-with-access s access) b (= (& qname b) qname qualified-name) :define-true)
          (return (& content b)))
        (<- s (& super s) :end-narrow))
      (return none))
    
    
    (%heading (3 :semantics) "Reading a Property")
    (deftag generic)
    (define (read-property (container (union obj-optional-limit frame)) (multiname multiname) (kind lookup-kind) (phase phase))
            object-opt
      (case container
        (:narrow (union undefined null boolean general-number character string namespace compound-attribute method-closure instance)
          (const c class (object-type container))
          (const qname qualified-name-opt (resolve-instance-member-name c multiname read phase))
          (if (and (in qname (tag none)) (in container dynamic-instance :narrow-true))
            (return (read-dynamic-property container multiname kind phase))
            (return (read-instance-member container c qname phase))))
        (:narrow (union system-frame global package parameter-frame block-frame)
          (const m static-member-opt (find-flat-member container multiname read phase))
          (if (and (in m (tag none)) (in container global :narrow-true))
            (return (read-dynamic-property container multiname kind phase))
            (return (read-static-member m phase))))
        (:narrow class
          (var this (union object (tag inaccessible none generic)))
          (case kind
            (:select (tag property-lookup)
              (<- this generic))
            (:narrow lexical-lookup
              (<- this (& this kind))))
          (const m2 (union (tag none) static-member qualified-name) (find-static-member container multiname read phase))
          (rwhen (not-in m2 qualified-name :narrow-both)
            (return (read-static-member m2 phase)))
          (case this
            (:select (tag none) (throw property-access-error))
            (:select (tag inaccessible) (throw compile-expression-error))
            (:select (tag generic)
              (todo))
            (:narrow object (return (read-instance-member this (object-type this) m2 phase)))))
        (:narrow prototype
          (return (read-dynamic-property container multiname kind phase)))
        (:narrow limited-instance
          (const superclass class-opt (& super (& limit container)))
          (rwhen (in superclass (tag none) :narrow-false)
            (return none))
          (const qname qualified-name-opt (resolve-instance-member-name superclass multiname read phase))
          (return (read-instance-member (& instance container) superclass qname phase)))))
    
    
    (define (read-instance-member (this object) (c class) (qname qualified-name-opt) (phase phase))
            object-opt
      (const m instance-member-opt (find-instance-member c qname read))
      (case m
        (:select (tag none)
          (return none))
        (:narrow instance-variable
          (rwhen (and (in phase (tag compile)) (not (& immutable m)))
            (throw compile-expression-error))
          (const v object-u (& value (find-slot this m)))
          (rwhen (in v (tag uninitialised) :narrow-false)
            (throw uninitialised-error))
          (return v))
        (:narrow instance-method
          (return (new method-closure this m)))
        (:narrow instance-getter
          (return ((& call m) this (& env m) phase)))
        (:narrow instance-setter
          (bottom (:local m) " cannot be an " (:type instance-setter) " because these are only represented as write-only members."))))
    
    
    (define (read-static-member (m static-member-opt) (phase phase)) object-opt
      (case m
        (:select (tag none) (return none))
        (:select (tag forbidden) (throw property-access-error))
        (:narrow variable (return (read-variable m phase)))
        (:narrow hoisted-var
          (rwhen (in phase (tag compile))
            (throw compile-expression-error))
          (const value (union object open-instance) (& value m))
          (assert (not-in value open-instance :narrow-true) "Note that " (:local value) " can be an " (:type open-instance) " only during the " (:tag compile)
                  " phase, which was ruled out above.")
          (return value))
        (:narrow constructor-method (return (& code m)))
        (:narrow getter
          (const env environment-i (& env m))
          (rwhen (in env (tag inaccessible) :narrow-false)
            (throw compile-expression-error))
          (return ((& call m) env phase)))
        (:narrow setter
          (bottom (:local m) " cannot be a " (:type setter) " because these are only represented as write-only members."))))
    
    
    (define (read-dynamic-property (container dynamic-object) (multiname multiname) (kind lookup-kind) (phase phase))
            object-opt
      (const name string-opt (select-public-name multiname))
      (rwhen (in name (tag none) :narrow-false)
        (return none))
      (rwhen (in phase (tag compile))
        (throw compile-expression-error))
      (reserve dp)
      (rwhen (some (& dynamic-properties container) dp (= (& name dp) name string) :define-true)
        (return (& value dp)))
      (when (in container prototype :narrow-true)
        (const parent prototype-opt (& parent container))
        (rwhen (not-in parent (tag none) :narrow-true)
          (return (read-dynamic-property parent multiname kind phase))))
      (rwhen (in kind (tag property-lookup))
        (return undefined))
      (return none))
    
    
    (define (read-variable (v variable) (phase phase)) object
      (rwhen (and (in phase (tag compile)) (not (& immutable v)))
        (throw compile-expression-error))
      (const value variable-value (& value v))
      (case value
        (:narrow object (return value))
        (:select (tag inaccessible)
          (if (in phase (tag compile))
            (throw compile-expression-error)
            (throw uninitialised-error)))
        (:select (tag uninitialised)
          (throw uninitialised-error))
        (:select open-instance
          (assert (in phase (tag compile)) "Note that an uninstantiated function can only be found when " (:assertion) ".")
          (throw compile-expression-error))
        (:narrow (-> () object)
          (assert (in phase (tag compile)) "Note that " (:assertion) " because all futures are resolved by the end of the compilation phase.")
          (&= value v inaccessible)
          (const type class (get-variable-type v phase))
          (const new-value object (value))
          (const coerced-value object (assignment-conversion new-value type))
          (&= value v coerced-value)
          (return new-value))))
    
    
    (%heading (3 :semantics) "Writing a Property")
    (define (write-property (container (union obj-optional-limit frame)) (multiname multiname) (kind lookup-kind) (create-if-missing boolean)
                            (new-value object) (phase (tag run)))
            (tag none ok)
      (case container
        (:select (union undefined null boolean general-number character string namespace compound-attribute method-closure)
          (return none))
        (:narrow (union system-frame global package parameter-frame block-frame)
          (const m static-member-opt (find-flat-member container multiname write phase))
          (if (and (in m (tag none)) (in container global :narrow-true))
            (return (write-dynamic-property container multiname create-if-missing new-value phase))
            (return (write-static-member m new-value phase))))
        (:narrow class
          (var this object-i-opt)
          (case kind
            (:select (tag property-lookup)
              (<- this none))
            (:narrow lexical-lookup
              (<- this (& this kind))))
          (const m2 (union (tag none) static-member qualified-name) (find-static-member container multiname write phase))
          (cond
           ((not-in m2 qualified-name :narrow-both)
            (return (write-static-member m2 new-value phase)))
           ((in this (tag none) :narrow-false)
            (throw property-access-error))
           ((in this (tag inaccessible) :narrow-false)
            (throw compile-expression-error))
           (nil (return (write-instance-member this (object-type this) m2 new-value phase)))))
        (:narrow prototype
          (return (write-dynamic-property container multiname create-if-missing new-value phase)))
        (:narrow instance
          (const c class (object-type container))
          (const qname qualified-name-opt (resolve-instance-member-name (object-type container) multiname write phase))
          (if (and (in qname (tag none)) (in container dynamic-instance :narrow-true))
            (return (write-dynamic-property container multiname create-if-missing new-value phase))
            (return (write-instance-member container c qname new-value phase))))
        (:narrow limited-instance
          (const superclass class-opt (& super (& limit container)))
          (rwhen (in superclass (tag none) :narrow-false)
            (return none))
          (const qname qualified-name-opt (resolve-instance-member-name superclass multiname write phase))
          (return (write-instance-member (& instance container) superclass qname new-value phase)))))
    
    
    (define (write-instance-member (this object) (c class) (qname qualified-name-opt) (new-value object) (phase (tag run)))
            (tag none ok)
      (const m instance-member-opt (find-instance-member c qname write))
      (case m
        (:select (tag none)
          (return none))
        (:narrow instance-variable
          (const s slot (find-slot this m))
          (rwhen (and (& immutable m) (not-in (& value s) (tag uninitialised)))
            (throw property-access-error))
          (const coerced-value object (assignment-conversion new-value (&opt type m)))
          (&= value s coerced-value)
          (return ok))
        (:select instance-method
          (throw property-access-error))
        (:narrow instance-getter
          (bottom (:local m) " cannot be an " (:type instance-getter) " because these are only represented as read-only members."))
        (:narrow instance-setter
          (const coerced-value object (assignment-conversion new-value (&opt type m)))
          ((& call m) this coerced-value (& env m) phase)
          (return ok))))
    
    
    (define (write-static-member (m static-member-opt) (new-value object) (phase (tag run))) (tag none ok)
      (case m
        (:select (tag none) (return none))
        (:select (union (tag forbidden) constructor-method) (throw property-access-error))
        (:narrow variable
          (write-variable m new-value phase)
          (return ok))
        (:narrow hoisted-var
          (&= value m new-value)
          (return ok))
        (:narrow getter
          (bottom (:local m) " cannot be a " (:type getter) " because these are only represented as read-only members."))
        (:narrow setter
          (const coerced-value object (assignment-conversion new-value (& type m)))
          (const env environment-i (& env m))
          (assert (not-in env (tag inaccessible) :narrow-true) "Note that all instances are resolved for the " (:tag run) " phase, so " (:assertion) ".")
          ((& call m) coerced-value env phase)
          (return ok))))
    
    
    (define (write-dynamic-property (container dynamic-object) (multiname multiname) (create-if-missing boolean) (new-value object) (phase (tag run)))
            (tag none ok)
      (const name string-opt (select-public-name multiname))
      (rwhen (in name (tag none) :narrow-false)
        (return none))
      (reserve dp)
      (rwhen (some (& dynamic-properties container) dp (= (& name dp) name string) :define-true)
        (&= value dp new-value)
        (return ok))
      (rwhen (not create-if-missing)
        (return none))
      (// "Before trying to create a new dynamic property, check that there is no read-only fixed property with the same name.")
      (var m (union (tag none) static-member qualified-name))
      (case container
        (:select prototype (<- m none))
        (:narrow dynamic-instance
          (<- m (resolve-instance-member-name (object-type container) multiname read phase)))
        (:narrow global
          (<- m (find-flat-member container multiname read phase))))
      (rwhen (not-in m (tag none))
        (return none))
      (&= dynamic-properties container (set+ (& dynamic-properties container) (list-set (new dynamic-property name new-value))))
      (return ok))
    
    
    (define (get-variable-type (v variable) (phase phase)) class
      (const type variable-type (& type v))
      (case type
        (:narrow class (return type))
        (:select (tag inaccessible)
          (assert (in phase (tag compile)) "Note that this can only happen when " (:assertion)
                  " because the compilation phase ensures that all types are valid, so invalid types will not occur during the run phase.")
          (throw compile-expression-error))
        (:narrow (-> () class)
          (assert (in phase (tag compile)) "Note that " (:assertion) " because all futures are resolved by the end of the compilation phase.")
          (&= type v inaccessible)
          (const new-type class (type))
          (&= type v new-type)
          (return new-type))))
    
    
    (define (write-variable (v variable) (new-value object) (phase (tag run))) void
      (const type class (get-variable-type v phase))
      (rwhen (or (in (& value v) (tag inaccessible))
                 (and (& immutable v) (not-in (& value v) (tag uninitialised))))
        (throw property-access-error))
      (const coerced-value object (assignment-conversion new-value type))
      (&= value v coerced-value))
    
    
    (%heading (3 :semantics) "Deleting a Property")
    (define (delete-property (container (union obj-optional-limit frame)) (multiname multiname) (kind lookup-kind) (phase (tag run))) boolean-opt
      (case container
        (:narrow (union undefined null boolean general-number character string namespace compound-attribute method-closure instance)
          (const c class (object-type container))
          (const qname qualified-name-opt (resolve-instance-member-name c multiname read phase))
          (if (and (in qname (tag none)) (in container dynamic-instance :narrow-true))
            (return (delete-dynamic-property container multiname))
            (return (delete-instance-member c qname))))
        (:narrow (union system-frame global package parameter-frame block-frame)
          (const m static-member-opt (find-flat-member container multiname read phase))
          (if (and (in m (tag none)) (in container global :narrow-true))
            (return (delete-dynamic-property container multiname))
            (return (delete-static-member m))))
        (:narrow class
          (var this (union object (tag none generic)))
          (case kind
            (:select (tag property-lookup)
              (<- this generic))
            (:narrow lexical-lookup
              (<- this (assert-not-in (& this kind) (tag inaccessible)))
              (// "Note that " (:local this) " cannot be " (:tag inaccessible) " during the " (:tag run) " phase.")))
          (const m2 (union (tag none) static-member qualified-name) (find-static-member container multiname read phase))
          (rwhen (not-in m2 qualified-name :narrow-both)
            (return (delete-static-member m2)))
          (case this
            (:select (tag none) (throw property-access-error))
            (:select (tag generic) (return false))
            (:narrow object (return (delete-instance-member (object-type this) m2)))))
        (:narrow prototype
          (return (delete-dynamic-property container multiname)))
        (:narrow limited-instance
          (const superclass class-opt (& super (& limit container)))
          (rwhen (in superclass (tag none) :narrow-false)
            (return none))
          (const qname qualified-name-opt (resolve-instance-member-name superclass multiname read phase))
          (return (delete-instance-member superclass qname)))))
    
    
    (define (delete-instance-member (c class) (qname qualified-name-opt)) boolean-opt
      (const m instance-member-opt (find-instance-member c qname read))
      (rwhen (in m (tag none))
        (return none))
      (return false))
    
    
    (define (delete-static-member (m static-member-opt)) boolean-opt
      (case m
        (:select (tag none) (return none))
        (:select (tag forbidden) (throw property-access-error))
        (:select (union variable hoisted-var constructor-method getter setter) (return false))))
    
    
    (define (delete-dynamic-property (container dynamic-object) (multiname multiname)) boolean-opt
      (const name string-opt (select-public-name multiname))
      (rwhen (in name (tag none) :narrow-false)
        (return none))
      (reserve dp)
      (cond
       ((some (& dynamic-properties container) dp (= (& name dp) name string) :define-true)
        (&= dynamic-properties container (set- (& dynamic-properties container) (list-set dp)))
        (return true))
       (nil (return none))))
    
    
    
    (%heading (2 :semantics) "Invocation")
    (define (bad-construct (args argument-list :unused) (runtime-env environment :unused) (phase phase :unused)) object
      (throw property-access-error))
    
    
    
    (%heading (2 :semantics) "Pre-Evaluation")
    (defvar pre-evaluators (vector (-> () void)) (vector-of (-> () void)))
    
    
    
    (%heading 1 "Expressions")
    (grammar-argument :beta allow-in no-in)
    
    
    (%heading (2 :semantics) "Terminal Actions")
    
    (declare-action name $identifier string :action nil
      (terminal-action name $identifier identity))
    (declare-action value $number general-number :action nil
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
         (const multiname multiname (map (& open-namespaces cxt) ns (new qualified-name ns (name :identifier))))
         (const a object (lexical-read env multiname compile))
         (rwhen (not-in a namespace :narrow-false) (throw bad-value-error))
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
         (const multiname multiname (map (& open-namespaces cxt) ns (new qualified-name ns (name :identifier))))
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
         (rwhen (not-in q namespace :narrow-false) (throw bad-value-error))
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
    
    
    (%heading 2 "Primary Expressions")
    (rule :primary-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :primary-expression (null) primary-expression-null
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return null)))
      (production :primary-expression (true) primary-expression-true
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return true)))
      (production :primary-expression (false) primary-expression-false
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return false)))
      (production :primary-expression (public) primary-expression-public
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return public-namespace)))
      (production :primary-expression ($number) primary-expression-number
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return (value $number))))
      (production :primary-expression ($string) primary-expression-string
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return (value $string))))
      (production :primary-expression (this) primary-expression-this
        ((validate (cxt :unused) env)
         (rwhen (in (find-this env true) (tag none))
           (throw syntax-error)))
        ((eval env (phase :unused))
         (const this object-i-opt (find-this env true))
         (assert (not-in this (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local this) " cannot be " (:tag none) " at this point.")
         (rwhen (in this (tag inaccessible) :narrow-false)
           (throw compile-expression-error))
         (return this)))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (todo)))
      (production :primary-expression (:paren-list-expression) primary-expression-paren-list-expression
        ((validate cxt env) ((validate :paren-list-expression) cxt env))
        ((eval env phase) (return ((eval :paren-list-expression) env phase))))
      (production :primary-expression (:array-literal) primary-expression-array-literal
        ((validate (cxt :unused) (env :unused)) (todo))
        ((eval (env :unused) (phase :unused)) (todo)))
      (production :primary-expression (:object-literal) primary-expression-object-literal
        ((validate cxt env) ((validate :object-literal) cxt env))
        ((eval env phase) (return ((eval :object-literal) env phase))))
      (production :primary-expression (:function-expression) primary-expression-function-expression
        ((validate cxt env) ((validate :function-expression) cxt env))
        ((eval env phase) (return ((eval :function-expression) env phase)))))
    
    (rule :paren-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :paren-expression (\( (:assignment-expression allow-in) \)) paren-expression-assignment-expression
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :assignment-expression) env phase)))))
    
    (rule :paren-list-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref))
                                  (eval-as-list (-> (environment phase) (vector object))))
      (production :paren-list-expression (:paren-expression) paren-list-expression-paren-expression
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :paren-expression) env phase)))
        ((eval-as-list env phase)
         (const r obj-or-ref ((eval :paren-expression) env phase))
         (const elt object (read-reference r phase))
         (return (vector elt))))
      (production :paren-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) paren-list-expression-list-expression
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :list-expression) env phase))
         (exec (read-reference ra phase))
         (const rb obj-or-ref ((eval :assignment-expression) env phase))
         (return (read-reference rb phase)))
        ((eval-as-list env phase)
         (const elts (vector object) ((eval-as-list :list-expression) env phase))
         (const r obj-or-ref ((eval :assignment-expression) env phase))
         (const elt object (read-reference r phase))
         (return (append elts (vector elt))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Function Expressions")
    (rule :function-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :function-expression (function :function-signature :block) function-expression-anonymous
        ((validate (cxt :unused) (env :unused)) (todo)) ;***** Clear break and continue inside cxt
        ((eval (env :unused) (phase :unused)) (todo)))
      (production :function-expression (function :identifier :function-signature :block) function-expression-named
        ((validate (cxt :unused) (env :unused)) (todo)) ;***** Clear break and continue inside cxt
        ((eval (env :unused) (phase :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Object Literals")
    (rule :object-literal ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :object-literal (\{ \}) object-literal-empty
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) phase)
         (rwhen (in phase (tag compile))
           (throw compile-expression-error))
         (return (new prototype object-prototype (list-set-of dynamic-property)))))
      (production :object-literal (\{ :field-list \}) object-literal-list
        ((validate cxt env) (exec ((validate :field-list) cxt env)))
        ((eval env phase)
         (rwhen (in phase (tag compile))
           (throw compile-expression-error))
         (const properties (list-set dynamic-property) ((eval :field-list) env phase))
         (return (new prototype object-prototype properties)))))
    
    (rule :field-list ((validate (-> (context environment) (list-set string))) (eval (-> (environment phase) (list-set dynamic-property))))
      (production :field-list (:literal-field) field-list-one
        ((validate cxt env) (return ((validate :literal-field) cxt env)))
        ((eval env phase)
         (const na named-argument ((eval :literal-field) env phase))
         (return (list-set (new dynamic-property (& name na) (& value na))))))
      (production :field-list (:field-list \, :literal-field) field-list-more
        ((validate cxt env)
         (const names1 (list-set string) ((validate :field-list) cxt env))
         (const names2 (list-set string) ((validate :literal-field) cxt env))
         (rwhen (nonempty (set* names1 names2))
           (throw syntax-error))
         (return (set+ names1 names2)))
        ((eval env phase)
         (const properties (list-set dynamic-property) ((eval :field-list) env phase))
         (const na named-argument ((eval :literal-field) env phase))
         (rwhen (some properties p (= (& name p) (& name na) string))
           (throw argument-mismatch-error))
         (return (set+ properties (list-set (new dynamic-property (& name na) (& value na))))))))
    
    (rule :literal-field ((validate (-> (context environment) (list-set string))) (eval (-> (environment phase) named-argument)))
      (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression
        ((validate cxt env)
         (const names (list-set string) ((validate :field-name) cxt env))
         ((validate :assignment-expression) cxt env)
         (return names))
        ((eval env phase)
         (const name string ((eval :field-name) env phase))
         (const r obj-or-ref ((eval :assignment-expression) env phase))
         (const value object (read-reference r phase))
         (return (new named-argument name value)))))
    
    (rule :field-name ((validate (-> (context environment) (list-set string))) (eval (-> (environment phase) string)))
      (production :field-name (:identifier) field-name-identifier
        ((validate (cxt :unused) (env :unused)) (return (list-set (name :identifier))))
        ((eval (env :unused) (phase :unused)) (return (name :identifier))))
      (production :field-name ($string) field-name-string
        ((validate (cxt :unused) (env :unused)) (return (list-set (value $string))))
        ((eval (env :unused) (phase :unused)) (return (value $string))))
      (production :field-name ($number) field-name-number
        ((validate (cxt :unused) (env :unused)) (return (list-set (to-string (value $number) compile))))
        ((eval (env :unused) (phase :unused)) (return (to-string (value $number) compile))))
      (? js2
        (production :field-name (:paren-expression) field-name-paren-expression
          ((validate cxt env)
           ((validate :paren-expression) cxt env)
           (return (list-set-of string)))
          ((eval env phase)
           (const r obj-or-ref ((eval :paren-expression) env phase))
           (const a object (read-reference r phase))
           (return (to-string a phase))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Super Expressions")
    (rule :super-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-optional-limit)))
      (production :super-expression (super) super-expression-super
        ((validate (cxt :unused) env)
         (rwhen (or (in (get-enclosing-class env) (tag none)) (in (find-this env false) (tag none)))
           (throw syntax-error)))
        ((eval env phase)
         (const this object-i-opt (find-this env false))
         (assert (not-in this (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local this) " cannot be " (:tag none) " at this point.")
         (rwhen (in this (tag inaccessible) :narrow-false)
           (throw compile-expression-error))
         (const limit class-opt (get-enclosing-class env))
         (assert (not-in limit (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local limit) " cannot be " (:tag none) " at this point.")
         (return (read-limited-reference this limit phase))))
      (production :super-expression (super :paren-expression) super-expression-super-paren-expression
        ((validate cxt env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw syntax-error))
         ((validate :paren-expression) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :paren-expression) env phase))
         (const limit class-opt (get-enclosing-class env))
         (assert (not-in limit (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local limit) " cannot be " (:tag none) " at this point.")
         (return (read-limited-reference r limit phase)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%text :comment (:global-call read-limited-reference r phase) " reads the reference, if any, inside " (:local r)
           " and returns the result, retaining " (:local limit)
           ". The object read from the reference is checked to make sure that it is an instance of " (:local limit)
           " or one of its descendants. If "
           (:local phase) " is " (:tag compile) ", only compile-time expressions can be evaluated in the process of reading " (:local r) ".")
    (define (read-limited-reference (r obj-or-ref) (limit class) (phase phase)) obj-optional-limit
      (const o object (read-reference r phase))
      (rwhen (= o null object)
        (return null))
      (rwhen (or (not-in o instance :narrow-false) (not (has-type o limit)))
        (throw bad-value-error))
      (return (new limited-instance o limit)))
    
    
    (%heading 2 "Postfix Expressions")
    (rule :postfix-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :attribute-expression) env phase))))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :full-postfix-expression) env phase))))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :short-new-expression) env phase)))))
    
    (rule :attribute-expression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier
        ((validate cxt env)
         ((validate :simple-qualified-identifier) cxt env)
         (action<- (strict :attribute-expression 0) (& strict cxt)))
        ((eval env (phase :unused))
         (return (new lexical-reference env (multiname :simple-qualified-identifier) (strict :attribute-expression 0)))))
      (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator
        ((validate cxt env)
         ((validate :attribute-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :attribute-expression) env phase))
         (const a object (read-reference r phase))
         (return ((eval :member-operator) env a phase))))
      (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call
        ((validate cxt env)
         ((validate :attribute-expression) cxt env)
         ((validate :arguments) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :attribute-expression) env phase))
         (const f object (read-reference r phase))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env phase))
         (return (call base f args phase)))))
    
    (rule :full-postfix-expression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression
        ((validate cxt env) ((validate :primary-expression) cxt env))
        ((eval env phase) (return ((eval :primary-expression) env phase))))
      (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier
        ((validate cxt env)
         ((validate :expression-qualified-identifier) cxt env)
         (action<- (strict :full-postfix-expression 0) (& strict cxt)))
        ((eval env (phase :unused))
         (return (new lexical-reference env (multiname :expression-qualified-identifier) (strict :full-postfix-expression 0)))))
      (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression
        ((validate cxt env) ((validate :full-new-expression) cxt env))
        ((eval env phase) (return ((eval :full-new-expression) env phase))))
      (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator
        ((validate cxt env)
         ((validate :full-postfix-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :full-postfix-expression) env phase))
         (const a object (read-reference r phase))
         (return ((eval :member-operator) env a phase))))
      (production :full-postfix-expression (:super-expression :member-operator) full-postfix-expression-super-member-operator
        ((validate cxt env)
         ((validate :super-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env phase)
         (const a obj-optional-limit ((eval :super-expression) env phase))
         (return ((eval :member-operator) env a phase))))
      (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call
        ((validate cxt env)
         ((validate :full-postfix-expression) cxt env)
         ((validate :arguments) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :full-postfix-expression) env phase))
         (const f object (read-reference r phase))
         (const base object (reference-base r))
         (const args argument-list ((eval :arguments) env phase))
         (return (call base f args phase))))
      (production :full-postfix-expression (:postfix-expression :no-line-break ++) full-postfix-expression-increment
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (const a object (read-reference r phase))
         (const b object (plus a phase))
         (const c object (add b 1.0 phase))
         (write-reference r c phase)
         (return b)))
      (production :full-postfix-expression (:postfix-expression :no-line-break --) full-postfix-expression-decrement
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (const a object (read-reference r phase))
         (const b object (plus a phase))
         (const c object (subtract b 1.0 phase))
         (write-reference r c phase)
         (return b))))
    
    (rule :full-new-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new
        ((validate cxt env) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :full-new-subexpression) env phase))
         (const f object (read-reference r phase))
         (const args argument-list ((eval :arguments) env phase))
         (return (construct f args phase)))))
    
    (rule :full-new-subexpression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression
        ((validate cxt env) ((validate :primary-expression) cxt env))
        ((eval env phase) (return ((eval :primary-expression) env phase))))
      (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier
        ((validate cxt env)
         ((validate :qualified-identifier) cxt env)
         (action<- (strict :full-new-subexpression 0) (& strict cxt)))
        ((eval env (phase :unused))
         (return (new lexical-reference env (multiname :qualified-identifier) (strict :full-new-subexpression 0)))))
      (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression
        ((validate cxt env) ((validate :full-new-expression) cxt env))
        ((eval env phase) (return ((eval :full-new-expression) env phase))))
      (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator
        ((validate cxt env)
         ((validate :full-new-subexpression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :full-new-subexpression) env phase))
         (const a object (read-reference r phase))
         (return ((eval :member-operator) env a phase))))
      (production :full-new-subexpression (:super-expression :member-operator) full-new-subexpression-super-member-operator
        ((validate cxt env)
         ((validate :super-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((eval env phase)
         (const a obj-optional-limit ((eval :super-expression) env phase))
         (return ((eval :member-operator) env a phase)))))
    
    (rule :short-new-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :short-new-expression (new :short-new-subexpression) short-new-expression-new
        ((validate cxt env) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :short-new-subexpression) env phase))
         (const f object (read-reference r phase))
         (return (construct f (new argument-list (vector-of object) (list-set-of named-argument)) phase)))))
    
    (rule :short-new-subexpression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :full-new-subexpression) env phase))))
      (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :short-new-expression) env phase)))))
    (%print-actions ("Validation" strict validate) ("Evaluation" eval))
    
    
    (%text :comment (:global-call reference-base r) " returns " (:type reference) " " (:local r) :apostrophe "s base or "
           (:tag null) " if there is none. The base" :apostrophe "s limit, if any, is ignored.")
    (define (reference-base (r obj-or-ref)) object
      (case r
        (:select (union object lexical-reference) (return null))
        (:narrow (union dot-reference bracket-reference)
          (const o obj-optional-limit (& base r))
          (case o
            (:narrow object (return o))
            (:narrow limited-instance (return (& instance o)))))))
    
    
    (define (call (this object) (a object) (args argument-list) (phase phase)) object
      (case a
        (:select (union undefined null boolean general-number character string namespace compound-attribute prototype package global) (throw bad-value-error))
        (:narrow class (return ((& call a) this args phase)))
        (:narrow instance
          (// "Note that " (:global resolve-alias) " is not called when getting the " (:label instance env) " field.")
          (return ((& call (resolve-alias a)) this args (& env a) phase)))
        (:narrow method-closure
          (const code instance (& code (& method a)))
          (return (call (& this a) code args phase)))))
    
    (define (construct (a object) (args argument-list) (phase phase)) object
      (case a
        (:select (union undefined null boolean general-number character string namespace compound-attribute method-closure prototype package global) (throw bad-value-error))
        (:narrow class (return ((& construct a) args phase)))
        (:narrow instance
          (// "Note that " (:global resolve-alias) " is not called when getting the " (:label instance env) " field.")
          (return ((& construct (resolve-alias a)) args (& env a) phase)))))
    
    
    (%heading 2 "Member Operators")
    (rule :member-operator ((validate (-> (context environment) void)) (eval (-> (environment obj-optional-limit phase) obj-or-ref)))
      (production :member-operator (\. :qualified-identifier) member-operator-qualified-identifier
        ((validate cxt env) :forward)
        ((eval (env :unused) base (phase :unused)) (return (new dot-reference base (multiname :qualified-identifier)))))
      (production :member-operator (:brackets) member-operator-brackets
        ((validate cxt env) :forward)
        ((eval env base phase)
         (const args argument-list ((eval :brackets) env phase))
         (return (new bracket-reference base args)))))
    
    (rule :brackets ((validate (-> (context environment) void)) (eval (-> (environment phase) argument-list)))
      (production :brackets ([ ]) brackets-none
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed
        ((validate cxt env) ((validate :list-expression) cxt env))
        ((eval env phase)
         (const positional (vector object) ((eval-as-list :list-expression) env phase))
         (return (new argument-list positional (list-set-of named-argument)))))
      (production :brackets ([ :named-argument-list ]) brackets-named
        ((validate cxt env) (exec ((validate :named-argument-list) cxt env)))
        ((eval env phase) (return ((eval :named-argument-list) env phase)))))
    
    (rule :arguments ((validate (-> (context environment) void)) (eval (-> (environment phase) argument-list)))
      (production :arguments (:paren-expressions) arguments-paren-expressions
        ((validate cxt env) ((validate :paren-expressions) cxt env))
        ((eval env phase) (return ((eval :paren-expressions) env phase))))
      (production :arguments (\( :named-argument-list \)) arguments-named
        ((validate cxt env) (exec ((validate :named-argument-list) cxt env)))
        ((eval env phase) (return ((eval :named-argument-list) env phase)))))
    
    (rule :paren-expressions ((validate (-> (context environment) void)) (eval (-> (environment phase) argument-list)))
      (production :paren-expressions (\( \)) paren-expressions-none
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (return (new argument-list (vector-of object) (list-set-of named-argument)))))
      (production :paren-expressions (:paren-list-expression) paren-expressions-some
        ((validate cxt env) :forward)
        ((eval env phase)
         (const positional (vector object) ((eval-as-list :paren-list-expression) env phase))
         (return (new argument-list positional (list-set-of named-argument))))))
    
    (rule :named-argument-list ((validate (-> (context environment) (list-set string))) (eval (-> (environment phase) argument-list)))
      (production :named-argument-list (:literal-field) named-argument-list-one
        ((validate cxt env) (return ((validate :literal-field) cxt env)))
        ((eval env phase)
         (const na named-argument ((eval :literal-field) env phase))
         (return (new argument-list (vector-of object) (list-set na)))))
      (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed
        ((validate cxt env)
         ((validate :list-expression) cxt env)
         (return ((validate :literal-field) cxt env)))
        ((eval env phase)
         (const positional (vector object) ((eval-as-list :list-expression) env phase))
         (const na named-argument ((eval :literal-field) env phase))
         (return (new argument-list positional (list-set na)))))
      (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more
        ((validate cxt env)
         (const names1 (list-set string) ((validate :named-argument-list) cxt env))
         (const names2 (list-set string) ((validate :literal-field) cxt env))
         (rwhen (nonempty (set* names1 names2))
           (throw syntax-error))
         (return (set+ names1 names2)))
        ((eval env phase)
         (const args argument-list ((eval :named-argument-list) env phase))
         (const na named-argument ((eval :literal-field) env phase))
         (rwhen (some (& named args) na2 (= (& name na2) (& name na) string))
           (throw argument-mismatch-error))
         (return (new argument-list (& positional args) (set+ (& named args) (list-set na)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Unary Operators")
    (rule :unary-expression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :unary-expression (:postfix-expression) unary-expression-postfix
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((eval env phase) (return ((eval :postfix-expression) env phase))))
      (production :unary-expression (delete :postfix-expression) unary-expression-delete
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         (action<- (strict :unary-expression 0) (& strict cxt)))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (return (delete-reference r (strict :unary-expression 0) phase))))
      (production :unary-expression (void :unary-expression) unary-expression-void
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :unary-expression) env phase))
         (exec (read-reference r phase))
         (return undefined)))
      (production :unary-expression (typeof :unary-expression) unary-expression-typeof
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :unary-expression) env phase))
         (const a object (read-reference r phase))
         (case a
           (:select undefined (return "undefined"))
           (:select (union null prototype package global) (return "object"))
           (:select boolean (return "boolean"))
           (:select long (return "long"))
           (:select u-long (return "ulong"))
           (:select float32 (return "float"))
           (:select float64 (return "number"))
           (:select character (return "character"))
           (:select string (return "string"))
           (:select namespace (return "namespace"))
           (:select compound-attribute (return "attribute"))
           (:select (union class method-closure) (return "function"))
           (:narrow instance (return (& typeof-string (resolve-alias a)))))))
      (production :unary-expression (++ :postfix-expression) unary-expression-increment
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (const a object (read-reference r phase))
         (const b object (plus a phase))
         (const c object (add b 1.0 phase))
         (write-reference r c phase)
         (return c)))
      (production :unary-expression (-- :postfix-expression) unary-expression-decrement
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (const a object (read-reference r phase))
         (const b object (plus a phase))
         (const c object (subtract b 1.0 phase))
         (write-reference r c phase)
         (return c)))
      (production :unary-expression (+ :unary-expression) unary-expression-plus
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :unary-expression) env phase))
         (const a object (read-reference r phase))
         (return (plus a phase))))
      (production :unary-expression (- :unary-expression) unary-expression-minus
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :unary-expression) env phase))
         (const a object (read-reference r phase))
         (return (minus a phase))))
      (production :unary-expression (- $negated-min-long) unary-expression-min-long
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused))
         (return (new long (neg (expt 2 63))))))
      (production :unary-expression (~ :unary-expression) unary-expression-bitwise-not
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :unary-expression) env phase))
         (const a object (read-reference r phase))
         (return (bit-not a phase))))
      (production :unary-expression (! :unary-expression) unary-expression-logical-not
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((eval env phase)
         (const r obj-or-ref ((eval :unary-expression) env phase))
         (const a object (read-reference r phase))
         (return (logical-not a phase)))))
    (%print-actions ("Validation" strict validate) ("Evaluation" eval))
    
    
    (%text :comment (:global-call plus a phase) " returns the value of the unary expression " (:character-literal "+") (:local a) ". If "
           (:local phase) " is " (:tag compile) ", only compile-time operations are permitted.")
    (define (plus (a object) (phase phase)) object
      (return (to-general-number a phase)))
    
    (define (minus (a object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (return (general-number-negate x)))
    
    (define (general-number-negate (x general-number)) general-number
      (case x
        (:narrow long (return (integer-to-long (neg (& value x)))))
        (:narrow u-long (return (integer-to-u-long (neg (& value x)))))
        (:narrow float32 (return (float32-negate x)))
        (:narrow float64 (return (float64-negate x)))))
    
    (define (bit-not (a object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (case x
        (:narrow long
          (const i (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (& value x))
          (return (new long (bitwise-xor i -1))))
        (:narrow u-long
          (const i (integer-range 0 (- (expt 2 64) 1)) (& value x))
          (return (new u-long (bitwise-xor i (hex #xFFFFFFFFFFFFFFFF)))))
        (:narrow (union float32 float64)
          (const i (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer x)))
          (return (real-to-float64 (bitwise-xor i -1))))))
    
    (%text :comment (:global-call logical-not a phase) " returns the value of the unary expression " (:character-literal "!") (:local a) ". If "
           (:local phase) " is " (:tag compile) ", only compile-time operations are permitted.")
    (define (logical-not (a object) (phase phase)) object
      (return (not (to-boolean a phase))))
    
        
    (%heading 2 "Multiplicative Operators")
    (rule :multiplicative-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :unary-expression) env phase))))
      (production :multiplicative-expression (:multiplicative-expression * :unary-expression) multiplicative-expression-multiply
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :multiplicative-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :unary-expression) env phase))
         (const b object (read-reference rb phase))
         (return (multiply a b phase))))
      (production :multiplicative-expression (:multiplicative-expression / :unary-expression) multiplicative-expression-divide
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :multiplicative-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :unary-expression) env phase))
         (const b object (read-reference rb phase))
         (return (divide a b phase))))
      (production :multiplicative-expression (:multiplicative-expression % :unary-expression) multiplicative-expression-remainder
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :multiplicative-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :unary-expression) env phase))
         (const b object (read-reference rb phase))
         (return (remainder a b phase)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (multiply (a object) (b object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (const y general-number (to-general-number b phase))
      (when (or (in x (union long u-long)) (in y (union long u-long)))
        (const i integer-opt (check-integer x))
        (const j integer-opt (check-integer y))
        (rwhen (and (not-in i (tag none) :narrow-true) (not-in j (tag none) :narrow-true))
          (const k integer (* i j))
          (if (or (in x u-long) (in y u-long))
            (return (integer-to-u-long k))
            (return (integer-to-long k)))))
      (return (float64-multiply (to-float64 x) (to-float64 y))))
    
    (define (divide (a object) (b object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (const y general-number (to-general-number b phase))
      (when (or (in x (union long u-long)) (in y (union long u-long)))
        (const i integer-opt (check-integer x))
        (const j integer-opt (check-integer y))
        (rwhen (and (not-in i (tag none) :narrow-true) (not-in j (tag none) :narrow-true) (/= j 0))
          (const q rational (rat/ i j))
          (if (or (in x u-long) (in y u-long))
            (return (rational-to-u-long q))
            (return (rational-to-long q)))))
      (return (float64-divide (to-float64 x) (to-float64 y))))
    
    (define (remainder (a object) (b object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (const y general-number (to-general-number b phase))
      (when (or (in x (union long u-long)) (in y (union long u-long)))
        (const i integer-opt (check-integer x))
        (const j integer-opt (check-integer y))
        (rwhen (and (not-in i (tag none) :narrow-true) (not-in j (tag none) :narrow-true) (/= j 0))
          (const q rational (rat/ i j))
          (const k integer (if (>= q 0 rational) (floor q) (ceiling q)))
          (const r integer (- i (* j k)))
          (if (or (in x u-long) (in y u-long))
            (return (integer-to-u-long r))
            (return (integer-to-long r)))))
      (return (float64-remainder (to-float64 x) (to-float64 y))))
    
    
    (%heading 2 "Additive Operators")
    (rule :additive-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :multiplicative-expression) env phase))))
      (production :additive-expression (:additive-expression + :multiplicative-expression) additive-expression-add
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :additive-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :multiplicative-expression) env phase))
         (const b object (read-reference rb phase))
         (return (add a b phase))))
      (production :additive-expression (:additive-expression - :multiplicative-expression) additive-expression-subtract
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :additive-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :multiplicative-expression) env phase))
         (const b object (read-reference rb phase))
         (return (subtract a b phase)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (add (a object) (b object) (phase phase)) object
      (const ap primitive-object (to-primitive a null phase))
      (const bp primitive-object (to-primitive b null phase))
      (rwhen (or (in ap (union character string)) (in bp (union character string)))
        (return (append (to-string ap phase) (to-string bp phase))))
      (const x general-number (to-general-number ap phase))
      (const y general-number (to-general-number bp phase))
      (when (or (in x (union long u-long)) (in y (union long u-long)))
        (const i integer-opt (check-integer x))
        (const j integer-opt (check-integer y))
        (rwhen (and (not-in i (tag none) :narrow-true) (not-in j (tag none) :narrow-true))
          (const k integer (+ i j))
          (if (or (in x u-long) (in y u-long))
            (return (integer-to-u-long k))
            (return (integer-to-long k)))))
      (return (float64-add (to-float64 x) (to-float64 y))))
    
    (define (subtract (a object) (b object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (const y general-number (to-general-number b phase))
      (when (or (in x (union long u-long)) (in y (union long u-long)))
        (const i integer-opt (check-integer x))
        (const j integer-opt (check-integer y))
        (rwhen (and (not-in i (tag none) :narrow-true) (not-in j (tag none) :narrow-true))
          (const k integer (- i j))
          (if (or (in x u-long) (in y u-long))
            (return (integer-to-u-long k))
            (return (integer-to-long k)))))
      (return (float64-subtract (to-float64 x) (to-float64 y))))
    
    
    (%heading 2 "Bitwise Shift Operators")
    (rule :shift-expression ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :additive-expression) env phase))))
      (production :shift-expression (:shift-expression << :additive-expression) shift-expression-left
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :shift-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :additive-expression) env phase))
         (const b object (read-reference rb phase))
         (return (shift-left a b phase))))
      (production :shift-expression (:shift-expression >> :additive-expression) shift-expression-right-signed
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :shift-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :additive-expression) env phase))
         (const b object (read-reference rb phase))
         (return (shift-right a b phase))))
      (production :shift-expression (:shift-expression >>> :additive-expression) shift-expression-right-unsigned
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :shift-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :additive-expression) env phase))
         (const b object (read-reference rb phase))
         (return (shift-right-unsigned a b phase)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (shift-left (a object) (b object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (var count integer (truncate-to-integer (to-general-number b phase)))
      (case x
        (:narrow (union float32 float64)
          (var i (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer x)))
          (<- count (bitwise-and count (hex #x1F)))
          (<- i (signed-wrap32 (bitwise-shift i count)))
          (return (real-to-float64 i)))
        (:narrow long
          (<- count (bitwise-and count (hex #x3F)))
          (const i (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (signed-wrap64 (bitwise-shift (& value x) count)))
          (return (new long i)))
        (:narrow u-long
          (<- count (bitwise-and count (hex #x3F)))
          (const i (integer-range 0 (- (expt 2 64) 1)) (unsigned-wrap64 (bitwise-shift (& value x) count)))
          (return (new u-long i)))))
    
    (define (shift-right (a object) (b object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (var count integer (truncate-to-integer (to-general-number b phase)))
      (case x
        (:narrow (union float32 float64)
          (var i (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer x)))
          (<- count (bitwise-and count (hex #x1F)))
          (<- i (bitwise-shift i (neg count)))
          (return (real-to-float64 i)))
        (:narrow long
          (<- count (bitwise-and count (hex #x3F)))
          (const i (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (bitwise-shift (& value x) (neg count)))
          (return (new long i)))
        (:narrow u-long
          (<- count (bitwise-and count (hex #x3F)))
          (const i (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (bitwise-shift (signed-wrap64 (& value x)) (neg count)))
          (return (new u-long (unsigned-wrap64 i))))))
    
    (define (shift-right-unsigned (a object) (b object) (phase phase)) object
      (const x general-number (to-general-number a phase))
      (var count integer (truncate-to-integer (to-general-number b phase)))
      (case x
        (:narrow (union float32 float64)
          (var i (integer-range 0 (- (expt 2 32) 1)) (unsigned-wrap32 (truncate-to-integer x)))
          (<- count (bitwise-and count (hex #x1F)))
          (<- i (bitwise-shift i (neg count)))
          (return (real-to-float64 i)))
        (:narrow long
          (<- count (bitwise-and count (hex #x3F)))
          (const i (integer-range 0 (- (expt 2 64) 1)) (bitwise-shift (unsigned-wrap64 (& value x)) (neg count)))
          (return (new long (signed-wrap64 i))))
        (:narrow u-long
          (<- count (bitwise-and count (hex #x3F)))
          (const i (integer-range 0 (- (expt 2 64) 1)) (bitwise-shift (& value x) (neg count)))
          (return (new u-long i)))))
    
    
    (%heading 2 "Relational Operators")
    (rule (:relational-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
       ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :shift-expression) env phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) < :shift-expression) relational-expression-less
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :relational-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :shift-expression) env phase))
         (const b object (read-reference rb phase))
         (return (is-less a b phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) > :shift-expression) relational-expression-greater
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :relational-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :shift-expression) env phase))
         (const b object (read-reference rb phase))
         (return (is-less b a phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) <= :shift-expression) relational-expression-less-or-equal
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :relational-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :shift-expression) env phase))
         (const b object (read-reference rb phase))
         (return (is-less-or-equal a b phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) >= :shift-expression) relational-expression-greater-or-equal
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :relational-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :shift-expression) env phase))
         (const b object (read-reference rb phase))
         (return (is-less-or-equal b a phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) is :shift-expression) relational-expression-is
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (todo)))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression) relational-expression-in
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (is-less (a object) (b object) (phase phase)) boolean
      (const ap primitive-object (to-primitive a null phase))
      (const bp primitive-object (to-primitive b null phase))
      (rwhen (and (in ap (union character string) :narrow-true) (in bp (union character string) :narrow-true))
        (return (< (to-string ap phase) (to-string bp phase) string)))
      (return (= (general-number-compare (to-general-number ap phase) (to-general-number bp phase)) less order)))
    
    (define (is-less-or-equal (a object) (b object) (phase phase)) boolean
      (const ap primitive-object (to-primitive a null phase))
      (const bp primitive-object (to-primitive b null phase))
      (rwhen (and (in ap (union character string) :narrow-true) (in bp (union character string) :narrow-true))
        (return (<= (to-string ap phase) (to-string bp phase) string)))
      (return (in (general-number-compare (to-general-number ap phase) (to-general-number bp phase)) (tag less equal))))
    
    
    (%heading 2 "Equality Operators")
    (rule (:equality-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :relational-expression) env phase))))
      (production (:equality-expression :beta) ((:equality-expression :beta) == (:relational-expression :beta)) equality-expression-equal
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :equality-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :relational-expression) env phase))
         (const b object (read-reference rb phase))
         (return (is-equal a b phase))))
      (production (:equality-expression :beta) ((:equality-expression :beta) != (:relational-expression :beta)) equality-expression-not-equal
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :equality-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :relational-expression) env phase))
         (const b object (read-reference rb phase))
         (const c boolean (is-equal a b phase))
         (return (not c))))
      (production (:equality-expression :beta) ((:equality-expression :beta) === (:relational-expression :beta)) equality-expression-strict-equal
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :equality-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :relational-expression) env phase))
         (const b object (read-reference rb phase))
         (return (is-strictly-equal a b phase))))
      (production (:equality-expression :beta) ((:equality-expression :beta) !== (:relational-expression :beta)) equality-expression-strict-not-equal
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :equality-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :relational-expression) env phase))
         (const b object (read-reference rb phase))
         (const c boolean (is-strictly-equal a b phase))
         (return (not c)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (is-equal (a object) (b object) (phase phase)) boolean
      (case a
        (:select (union undefined null)
          (return (in b (union undefined null))))
        (:narrow boolean
          (if (in b boolean :narrow-true)
            (return (= a b boolean))
            (return (is-equal (to-general-number a phase) b phase))))
        (:narrow general-number
          (const bp primitive-object (to-primitive b null phase))
          (case bp
            (:select (union undefined null) (return false))
            (:select (union boolean general-number character string) (return (= (general-number-compare a (to-general-number bp phase)) equal order)))))
        (:narrow (union character string)
          (const bp primitive-object (to-primitive b null phase))
          (case bp
            (:select (union undefined null) (return false))
            (:select (union boolean general-number) (return (= (general-number-compare (to-general-number a phase) (to-general-number bp phase)) equal order)))
            (:narrow (union character string) (return (= (to-string a phase) (to-string bp phase) string)))))
        (:select (union namespace compound-attribute class method-closure prototype instance package global)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace compound-attribute class method-closure prototype instance package global) (return (is-strictly-equal a b phase)))
            (:select (union boolean general-number character string)
              (const ap primitive-object (to-primitive a null phase))
              (return (is-equal ap b phase)))))))
    
    (define (is-strictly-equal (a object) (b object) (phase phase)) boolean
      (cond
       ((in a alias-instance :narrow-true)
        (return (is-strictly-equal (& original a) b phase)))
       ((in b alias-instance :narrow-true)
        (return (is-strictly-equal a (& original b) phase)))
       ((and (in a general-number :narrow-true) (in b general-number :narrow-true))
        (return (= (general-number-compare a b) equal order)))
       (nil
        (return (= a b object)))))
    
    
    (%heading 2 "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :equality-expression) env phase))))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression :beta) & (:equality-expression :beta)) bitwise-and-expression-and
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :bitwise-and-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :equality-expression) env phase))
         (const b object (read-reference rb phase))
         (return (bit-and a b phase)))))
    
    (rule (:bitwise-xor-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :bitwise-and-expression) env phase))))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression :beta) ^ (:bitwise-and-expression :beta)) bitwise-xor-expression-xor
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :bitwise-xor-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :bitwise-and-expression) env phase))
         (const b object (read-reference rb phase))
         (return (bit-xor a b phase)))))
    
    (rule (:bitwise-or-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :bitwise-xor-expression) env phase))))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| (:bitwise-xor-expression :beta)) bitwise-or-expression-or
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :bitwise-or-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :bitwise-xor-expression) env phase))
         (const b object (read-reference rb phase))
         (return (bit-or a b phase)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    (define (bit-and (a object) (b object) (phase phase)) general-number
      (const x general-number (to-general-number a phase))
      (const y general-number (to-general-number b phase))
      (cond
       ((or (in x (union long u-long) :narrow-false) (in y (union long u-long) :narrow-false))
        (const i (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (signed-wrap64 (truncate-to-integer x)))
        (const j (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (signed-wrap64 (truncate-to-integer y)))
        (const k (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (bitwise-and i j))
        (if (or (in x u-long) (in y u-long))
          (return (new u-long (unsigned-wrap64 k)))
          (return (new long k))))
       (nil
        (const i (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer x)))
        (const j (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer y)))
        (return (real-to-float64 (bitwise-and i j))))))
    
    (define (bit-xor (a object) (b object) (phase phase)) general-number
      (const x general-number (to-general-number a phase))
      (const y general-number (to-general-number b phase))
      (cond
       ((or (in x (union long u-long) :narrow-false) (in y (union long u-long) :narrow-false))
        (const i (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (signed-wrap64 (truncate-to-integer x)))
        (const j (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (signed-wrap64 (truncate-to-integer y)))
        (const k (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (bitwise-xor i j))
        (if (or (in x u-long) (in y u-long))
          (return (new u-long (unsigned-wrap64 k)))
          (return (new long k))))
       (nil
        (const i (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer x)))
        (const j (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer y)))
        (return (real-to-float64 (bitwise-xor i j))))))
    
    (define (bit-or (a object) (b object) (phase phase)) general-number
      (const x general-number (to-general-number a phase))
      (const y general-number (to-general-number b phase))
      (cond
       ((or (in x (union long u-long) :narrow-false) (in y (union long u-long) :narrow-false))
        (const i (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (signed-wrap64 (truncate-to-integer x)))
        (const j (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (signed-wrap64 (truncate-to-integer y)))
        (const k (integer-range (neg (expt 2 63)) (- (expt 2 63) 1)) (bitwise-or i j))
        (if (or (in x u-long) (in y u-long))
          (return (new u-long (unsigned-wrap64 k)))
          (return (new long k))))
       (nil
        (const i (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer x)))
        (const j (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (truncate-to-integer y)))
        (return (real-to-float64 (bitwise-or i j))))))
    
    
    (%heading 2 "Binary Logical Operators")
    (rule (:logical-and-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :bitwise-or-expression) env phase))))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :logical-and-expression) env phase))
         (const a object (read-reference ra phase))
         (cond
          ((to-boolean a phase)
           (const rb obj-or-ref ((eval :bitwise-or-expression) env phase))
           (return (read-reference rb phase)))
          (nil (return a))))))
    
    (rule (:logical-xor-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :logical-and-expression) env phase))))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :logical-xor-expression) env phase))
         (const a object (read-reference ra phase))
         (const rb obj-or-ref ((eval :logical-and-expression) env phase))
         (const b object (read-reference rb phase))
         (const ba boolean (to-boolean a phase))
         (const bb boolean (to-boolean b phase))
         (return (xor ba bb)))))
    
    (rule (:logical-or-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :logical-xor-expression) env phase))))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :logical-or-expression) env phase))
         (const a object (read-reference ra phase))
         (cond
          ((to-boolean a phase) (return a))
          (nil
           (const rb obj-or-ref ((eval :logical-xor-expression) env phase))
           (return (read-reference rb phase)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Conditional Operator")
    (rule (:conditional-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :logical-or-expression) env phase))))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :logical-or-expression) env phase))
         (const a object (read-reference ra phase))
         (cond
          ((to-boolean a phase)
           (const rb obj-or-ref ((eval :assignment-expression 1) env phase))
           (return (read-reference rb phase)))
          (nil
           (const rc obj-or-ref ((eval :assignment-expression 2) env phase))
           (return (read-reference rc phase)))))))
    
    (rule (:non-assignment-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :logical-or-expression) env phase))))
      (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :logical-or-expression) env phase))
         (const a object (read-reference ra phase))
         (cond
          ((to-boolean a phase)
           (const rb obj-or-ref ((eval :non-assignment-expression 1) env phase))
           (return (read-reference rb phase)))
          (nil
           (const rc obj-or-ref ((eval :non-assignment-expression 2) env phase))
           (return (read-reference rc phase)))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Assignment Operators")
    (rule (:assignment-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        ((validate cxt env) ((validate :conditional-expression) cxt env))
        ((eval env phase) (return ((eval :conditional-expression) env phase))))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const ra obj-or-ref ((eval :postfix-expression) env phase))
         (const rb obj-or-ref ((eval :assignment-expression) env phase))
         (const b object (read-reference rb phase))
         (write-reference ra b phase)
         (return b)))
      (production (:assignment-expression :beta) (:postfix-expression :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r-left obj-or-ref ((eval :postfix-expression) env phase))
         (const o-left object (read-reference r-left phase))
         (const r-right obj-or-ref ((eval :assignment-expression) env phase))
         (const o-right object (read-reference r-right phase))
         (const result object ((op :compound-assignment) o-left o-right phase))
         (write-reference r-left result phase)
         (return result)))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r-left obj-or-ref ((eval :postfix-expression) env phase))
         (const o-left object (read-reference r-left phase))
         (const b-left boolean (to-boolean o-left phase))
         (var result object o-left)
         (case (operator :logical-assignment)
           (:select (tag and-eq)
             (when b-left
               (<- result (read-reference ((eval :assignment-expression) env phase) phase))))
           (:select (tag xor-eq)
             (const b-right boolean (to-boolean (read-reference ((eval :assignment-expression) env phase) phase) phase))
             (<- result (xor b-left b-right)))
           (:select (tag or-eq)
             (when (not b-left)
               (<- result (read-reference ((eval :assignment-expression) env phase) phase)))))
         (write-reference r-left result phase)
         (return result))))
    
    (rule :compound-assignment ((op (-> (object object phase) object)))
      (production :compound-assignment (*=) compound-assignment-multiply (op multiply))
      (production :compound-assignment (/=) compound-assignment-divide (op divide))
      (production :compound-assignment (%=) compound-assignment-remainder (op remainder))
      (production :compound-assignment (+=) compound-assignment-add (op add))
      (production :compound-assignment (-=) compound-assignment-subtract (op subtract))
      (production :compound-assignment (<<=) compound-assignment-shift-left (op shift-left))
      (production :compound-assignment (>>=) compound-assignment-shift-right (op shift-right))
      (production :compound-assignment (>>>=) compound-assignment-shift-right-unsigned (op shift-right-unsigned))
      (production :compound-assignment (&=) compound-assignment-bit-and (op bit-and))
      (production :compound-assignment (^=) compound-assignment-bit-xor (op bit-xor))
      (production :compound-assignment (\|=) compound-assignment-bit-or (op bit-or)))
    
    (rule :logical-assignment ((operator (tag and-eq xor-eq or-eq)))
      (production :logical-assignment (&&=) logical-assignment-logical-and (operator and-eq))
      (production :logical-assignment (^^=) logical-assignment-logical-xor (operator xor-eq))
      (production :logical-assignment (\|\|=) logical-assignment-logical-or (operator or-eq)))
    
    (deftag and-eq)
    (deftag xor-eq)
    (deftag or-eq)
    (%print-actions ("Validation" validate) ("Evaluation" op operator eval))
    
    
    (%heading 2 "Comma Expressions")
    (rule (:list-expression :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) obj-or-ref))
                                    (eval-as-list (-> (environment phase) (vector object))))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :assignment-expression) env phase)))
        ((eval-as-list env phase)
         (const r obj-or-ref ((eval :assignment-expression) env phase))
         (const elt object (read-reference r phase))
         (return (vector elt))))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((validate cxt env) :forward)
        ((eval env phase)
         (const ra obj-or-ref ((eval :list-expression) env phase))
         (exec (read-reference ra phase))
         (const rb obj-or-ref ((eval :assignment-expression) env phase))
         (return (read-reference rb phase)))
        ((eval-as-list env phase)
         (const elts (vector object) ((eval-as-list :list-expression) env phase))
         (const r obj-or-ref ((eval :assignment-expression) env phase))
         (const elt object (read-reference r phase))
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
           (throw bad-value-error))
         (return o))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 1 "Statements")
    
    (grammar-argument :omega
                      abbrev       ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)        ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (rule (:statement :omega) ((validate (-> (context environment (list-set label) jump-targets plurality) void)) (eval (-> (environment object) object)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :expression-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :expression-statement) env))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :super-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :super-statement) env))))
      (production (:statement :omega) (:block) statement-block
        ((validate cxt env (sl :unused) jt pl) ((validate :block) cxt env jt pl))
        ((eval env d) (return ((eval :block) env d))))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        ((validate cxt env sl jt (pl :unused)) ((validate :labeled-statement) cxt env sl jt))
        ((eval env d) (return ((eval :labeled-statement) env d))))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        ((validate cxt env (sl :unused) jt (pl :unused)) ((validate :if-statement) cxt env jt))
        ((eval env d) (return ((eval :if-statement) env d))))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused) (pl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((validate cxt env sl jt (pl :unused)) ((validate :do-statement) cxt env sl jt))
        ((eval env d) (return ((eval :do-statement) env d))))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((validate cxt env sl jt (pl :unused)) ((validate :while-statement) cxt env sl jt))
        ((eval env d) (return ((eval :while-statement) env d))))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused) (pl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused) (pl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) jt (pl :unused)) ((validate :continue-statement) jt))
        ((eval env d) (return ((eval :continue-statement) env d))))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) jt (pl :unused)) ((validate :break-statement) jt))
        ((eval env d) (return ((eval :break-statement) env d))))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :return-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :return-statement) env))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :throw-statement) cxt env))
        ((eval env (d :unused)) (return ((eval :throw-statement) env))))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused) (pl :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo))))
    
    
    (rule (:substatement :omega) ((enabled (writable-cell boolean)) (validate (-> (context environment (list-set label) jump-targets) void))
                                  (eval (-> (environment object) object)))
      (production (:substatement :omega) (:empty-statement) substatement-empty-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused)))
        ((eval (env :unused) d) (return d)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        ((validate cxt env sl jt) ((validate :statement) cxt env sl jt plural))
        ((eval env d) (return ((eval :statement) env d))))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((validate cxt env (sl :unused) (jt :unused)) ((validate :simple-variable-definition) cxt env))
        ((eval env d) (return ((eval :simple-variable-definition) env d))))
      (production (:substatement :omega) (:attributes :no-line-break { :substatements }) substatement-annotated-group
        ((validate cxt env (sl :unused) jt)
         ((validate :attributes) cxt env)
         (const attr attribute ((eval :attributes) env compile))
         (rwhen (not-in attr boolean :narrow-false)
           (throw bad-value-error))
         (action<- (enabled :substatement 0) attr)
         (when attr
           ((validate :substatements) cxt env jt)))
        ((eval env d)
         (if (enabled :substatement 0)
           (return ((eval :substatements) env d))
           (return d)))))
    
    
    (rule :substatements ((validate (-> (context environment jump-targets) void)) (eval (-> (environment object) object)))
      (production :substatements () substatements-none
        ((validate (cxt :unused) (env :unused) (jt :unused)))
        ((eval (env :unused) d) (return d)))
      (production :substatements (:substatements-prefix (:substatement abbrev)) substatements-more
        ((validate cxt env jt)
         ((validate :substatements-prefix) cxt env jt)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((eval env d)
         (const o object ((eval :substatements-prefix) env d))
         (return ((eval :substatement) env o)))))
    
    (rule :substatements-prefix ((validate (-> (context environment jump-targets) void)) (eval (-> (environment object) object)))
      (production :substatements-prefix () substatements-prefix-none
        ((validate (cxt :unused) (env :unused) (jt :unused)))
        ((eval (env :unused) d) (return d)))
      (production :substatements-prefix (:substatements-prefix (:substatement full)) substatements-prefix-more
        ((validate cxt env jt)
         ((validate :substatements-prefix) cxt env jt)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((eval env d)
         (const o object ((eval :substatements-prefix) env d))
         (return ((eval :substatement) env o)))))
    
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon no-short-if) () semicolon-no-short-if)
    (%print-actions ("Validation" enabled validate) ("Evaluation" eval))
    
    
    (%heading 2 "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%heading 2 "Expression Statement")
    (rule :expression-statement ((validate (-> (context environment) void)) (eval (-> (environment) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        ((validate cxt env) :forward)
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
    (rule :block ((compile-frame (writable-cell block-frame))
                  (validate (-> (context environment jump-targets plurality) void)) (validate-using-frame (-> (context environment jump-targets plurality frame) void))
                  (eval (-> (environment object) object)) (eval-using-frame (-> (environment frame object) object)))
      (production :block ({ :directives }) block-directives
        ((validate cxt env jt pl)
         (const compile-frame block-frame (new block-frame (list-set-of static-binding) (list-set-of static-binding) pl))
         (action<- (compile-frame :block 0) compile-frame)
         (exec ((validate :directives) cxt (cons compile-frame env) jt pl none)))
        ((validate-using-frame cxt env jt pl frame)
         (exec ((validate :directives) cxt (cons frame env) jt pl none)))
        ((eval env d)
         (const compile-frame block-frame (compile-frame :block 0))
         (var runtime-frame block-frame)
         (case (& plurality compile-frame)
           (:select (tag singular) (<- runtime-frame compile-frame))
           (:select (tag plural)
             (<- runtime-frame (new block-frame (list-set-of static-binding) (list-set-of static-binding) singular))
             (instantiate-frame compile-frame runtime-frame (cons runtime-frame env))))
         (return ((eval :directives) (cons runtime-frame env) d)))
        ((eval-using-frame env frame d)
         (return ((eval :directives) (cons frame env) d)))))
    (%print-actions ("Validation" validate validate-using-frame) ("Evaluation" eval eval-using-frame))
    
    
    (%heading 2 "Labeled Statements")
    (rule (:labeled-statement :omega) ((validate (-> (context environment (list-set label) jump-targets) void)) (eval (-> (environment object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((validate cxt env sl jt)
         (const name string (name :identifier))
         (rwhen (set-in name (& break-targets jt))
           (throw syntax-error))
         (const jt2 jump-targets (new jump-targets (set+ (& break-targets jt) (list-set-of label name)) (& continue-targets jt)))
         ((validate :substatement) cxt env (set+ sl (list-set-of label name)) jt2))
        ((eval env d)
         (catch ((return ((eval :substatement) env d)))
           (x) (if (and (in x break :narrow-true) (= (& label x) (name :identifier) label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "If Statement")
    (rule (:if-statement :omega) ((validate (-> (context environment jump-targets) void)) (eval (-> (environment object) object)))
      (production (:if-statement abbrev) (if :paren-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((eval env d)
         (const r obj-or-ref ((eval :paren-list-expression) env run))
         (const o object (read-reference r run))
         (if (to-boolean o run)
           (return ((eval :substatement) env d))
           (return d))))
      (production (:if-statement full) (if :paren-list-expression (:substatement full)) if-statement-if-then-full
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((eval env d)
         (const r obj-or-ref ((eval :paren-list-expression) env run))
         (const o object (read-reference r run))
         (if (to-boolean o run)
           (return ((eval :substatement) env d))
           (return d))))
      (production (:if-statement :omega) (if :paren-list-expression (:substatement no-short-if) else (:substatement :omega))
                  if-statement-if-then-else
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement 1) cxt env (list-set-of label) jt)
         ((validate :substatement 2) cxt env (list-set-of label) jt))
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
    (rule :do-statement ((labels (writable-cell (list-set label))) (validate (-> (context environment (list-set label) jump-targets) void))
                         (eval (-> (environment object) object)))
      (production :do-statement (do (:substatement abbrev) while :paren-list-expression) do-statement-do-while
        ((validate cxt env sl jt)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :do-statement 0) continue-labels)
         (const jt2 jump-targets (new jump-targets
                                   (set+ (& break-targets jt) (list-set-of label default))
                                   (set+ (& continue-targets jt) continue-labels)))
         ((validate :substatement) cxt env (list-set-of label) jt2)
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
    (rule (:while-statement :omega) ((labels (writable-cell (list-set label))) (validate (-> (context environment (list-set label) jump-targets) void))
                                     (eval (-> (environment object) object)))
      (production (:while-statement :omega) (while :paren-list-expression (:substatement :omega)) while-statement-while
        ((validate cxt env sl jt)
         ((validate :paren-list-expression) cxt env)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :while-statement 0) continue-labels)
         (const jt2 jump-targets (new jump-targets
                                   (set+ (& break-targets jt) (list-set-of label default))
                                   (set+ (& continue-targets jt) continue-labels)))
         ((validate :substatement) cxt env (list-set-of label) jt2))
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
    (rule :continue-statement ((validate (-> (jump-targets) void)) (eval (-> (environment object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((validate jt)
         (rwhen (set-not-in default (& continue-targets jt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new continue d default))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((validate jt)
         (rwhen (set-not-in (name :identifier) (& continue-targets jt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new continue d (name :identifier))))))
    
    (rule :break-statement ((validate (-> (jump-targets) void)) (eval (-> (environment object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((validate jt)
         (rwhen (set-not-in default (& break-targets jt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new break d default))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((validate jt)
         (rwhen (set-not-in (name :identifier) (& break-targets jt))
           (throw syntax-error)))
        ((eval (env :unused) d) (throw (new break d (name :identifier))))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Return Statement")
    (rule :return-statement ((validate (-> (context environment) void)) (eval (-> (environment) object)))
      (production :return-statement (return) return-statement-default
        ((validate (cxt :unused) env)
         (rwhen (not-in (get-regional-frame env) parameter-frame)
           (throw syntax-error)))
        ((eval (env :unused)) (throw (new returned-value undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((validate cxt env)
         (rwhen (not-in (get-regional-frame env) parameter-frame)
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
        ((validate cxt env) ((validate :list-expression) cxt env))
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
    (rule (:directive :omega_2) ((enabled (writable-cell boolean)) (validate (-> (context environment jump-targets plurality attribute-opt-not-false) context))
                                 (eval (-> (environment object) object)))
      (production (:directive :omega_2) (:empty-statement) directive-empty-statement
        ((validate cxt (env :unused) (jt :unused) (pl :unused) (attr :unused)) (return cxt))
        ((eval (env :unused) d) (return d)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        ((validate cxt env jt pl attr)
         (rwhen (not-in attr (tag none true))
           (throw syntax-error))
         ((validate :statement) cxt env (list-set-of label) jt pl)
         (return cxt))
        ((eval env d) (return ((eval :statement) env d))))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((validate cxt env (jt :unused) pl attr) (return ((validate :annotatable-directive) cxt env pl attr)))
        ((eval env d) (return ((eval :annotatable-directive) env d))))
      (production (:directive :omega_2) (:attributes :no-line-break (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((validate cxt env (jt :unused) pl attr)
         ((validate :attributes) cxt env)
         (const attr2 attribute ((eval :attributes) env compile))
         (const attr3 attribute (combine-attributes attr attr2))
         (action<- (enabled :directive 0) (not-in attr3 false-type))
         (if (not-in attr3 false-type :narrow-true)
           (return ((validate :annotatable-directive) cxt env pl attr3))
           (return cxt)))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :annotatable-directive) env d))
           (return d))))
      (production (:directive :omega_2) (:attributes :no-line-break { :directives }) directive-annotated-group
        ((validate cxt env jt pl attr)
         ((validate :attributes) cxt env)
         (const attr2 attribute ((eval :attributes) env compile))
         (const attr3 attribute (combine-attributes attr attr2))
         (action<- (enabled :directive 0) (not-in attr3 false-type))
         (rwhen (in attr3 false-type :narrow-false)
           (return cxt))
         (return ((validate :directives) cxt env jt pl attr3)))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :directives) env d))
           (return d))))
      (production (:directive :omega_2) (:package-definition) directive-package-definition
        ((validate (cxt :unused) (env :unused) (jt :unused) (pl :unused) attr) (if (in attr (tag none true)) (todo) (throw syntax-error)))
        ((eval (env :unused) (d :unused)) (todo)))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((validate (cxt :unused) (env :unused) (jt :unused) (pl :unused) attr) (if (in attr (tag none true)) (todo) (throw syntax-error)))
          ((eval (env :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((validate cxt (env :unused) (jt :unused) (pl :unused) attr) (if (in attr (tag none true)) (return ((validate :pragma) cxt)) (throw syntax-error)))
        ((eval (env :unused) d) (return d))))
    
    (rule (:annotatable-directive :omega_2) ((validate (-> (context environment plurality attribute-opt-not-false) context)) (eval (-> (environment object) object)))
      (production (:annotatable-directive :omega_2) (:export-definition (:semicolon :omega_2)) annotatable-directive-export-definition
        ((validate (cxt :unused) (env :unused) (pl :unused) (attr :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:variable-definition (:semicolon :omega_2)) annotatable-directive-variable-definition
        ((validate cxt env (pl :unused) attr)
         ((validate :variable-definition) cxt env attr)
         (return cxt))
        ((eval env d) (return ((eval :variable-definition) env d))))
      (production (:annotatable-directive :omega_2) (:function-definition) annotatable-directive-function-definition
        ((validate cxt env pl attr)
         ((validate :function-definition) cxt env pl attr)
         (return cxt))
        ((eval (env :unused) d) (return d)))
      (production (:annotatable-directive :omega_2) (:class-definition) annotatable-directive-class-definition
        ((validate cxt env pl attr)
         ((validate :class-definition) cxt env pl attr)
         (return cxt))
        ((eval env d) (return ((eval :class-definition) env d))))
      (production (:annotatable-directive :omega_2) (:namespace-definition (:semicolon :omega_2)) annotatable-directive-namespace-definition
        ((validate cxt env pl attr)
         ((validate :namespace-definition) cxt env pl attr)
         (return cxt))
        ((eval (env :unused) d) (return d)))
      ;(production (:annotatable-directive :omega_2) ((:interface-definition :omega_2)) annotatable-directive-interface-definition
      ;  ((validate (cxt :unused) (env :unused) (pl :unused) (attr :unused)) (todo))
      ;  ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:import-directive (:semicolon :omega_2)) annotatable-directive-import-directive
        ((validate (cxt :unused) (env :unused) (pl :unused) (attr :unused)) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:use-directive (:semicolon :omega_2)) annotatable-directive-use-directive
        ((validate cxt env (pl :unused) attr)
         (if (in attr (tag none true))
           (return ((validate :use-directive) cxt env))
           (throw syntax-error)))
        ((eval (env :unused) d) (return d))))
    
    
    (rule :directives ((validate (-> (context environment jump-targets plurality attribute-opt-not-false) context)) (eval (-> (environment object) object)))
      (production :directives () directives-none
        ((validate cxt (env :unused) (jt :unused) (pl :unused) (attr :unused)) (return cxt))
        ((eval (env :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((validate cxt env jt pl attr)
         (const cxt2 context ((validate :directives-prefix) cxt env jt pl attr))
         (return ((validate :directive) cxt2 env jt pl attr)))
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    
    (rule :directives-prefix ((validate (-> (context environment jump-targets plurality attribute-opt-not-false) context)) (eval (-> (environment object) object)))
      (production :directives-prefix () directives-prefix-none
        ((validate cxt (env :unused) (jt :unused) (pl :unused) (attr :unused)) (return cxt))
        ((eval (env :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((validate cxt env jt pl attr)
         (const cxt2 context ((validate :directives-prefix) cxt env jt pl attr))
         (return ((validate :directive) cxt2 env jt pl attr)))
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Attributes")
    (rule :attributes ((validate (-> (context environment) void)) (eval (-> (environment phase) attribute)))
      (production :attributes (:attribute) attributes-one
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :attribute) env phase))))
      (production :attributes (:attribute-combination) attributes-attribute-combination
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :attribute-combination) env phase)))))
    
    
    (rule :attribute-combination ((validate (-> (context environment) void)) (eval (-> (environment phase) attribute)))
      (production :attribute-combination (:attribute :no-line-break :attributes) attribute-combination-more
        ((validate cxt env) :forward)
        ((eval env phase)
         (const a attribute ((eval :attribute) env phase))
         (rwhen (in a false-type :narrow-false)
           (return false))
         (const b attribute ((eval :attributes) env phase))
         (return (combine-attributes a b)))))
    
    
    (rule :attribute ((validate (-> (context environment) void)) (eval (-> (environment phase) attribute)))
      (production :attribute (:attribute-expression) attribute-attribute-expression
        ((validate cxt env) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :attribute-expression) env phase))
         (const a object (read-reference r phase))
         (rwhen (not-in a attribute :narrow-false)
           (throw bad-value-error))
         (return a)))
      (production :attribute (true) attribute-true
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (return true)))
      (production :attribute (false) attribute-false
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (return false)))
      (production :attribute (public) attribute-public
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (return public-namespace)))
      (production :attribute (:nonexpression-attribute) attribute-nonexpression-attribute
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :nonexpression-attribute) env phase)))))
    
    
    (rule :nonexpression-attribute ((validate (-> (context environment) void)) (eval (-> (environment phase) attribute)))
      (production :nonexpression-attribute (final) nonexpression-attribute-final
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return (new compound-attribute (list-set-of namespace) false false final none false false))))
      (production :nonexpression-attribute (private) nonexpression-attribute-private
        ((validate (cxt :unused) env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw syntax-error)))
        ((eval env (phase :unused))
         (const c class-opt (get-enclosing-class env))
         (assert (not-in c (tag none) :narrow-true) "Note that " (:action validate) " ensured that " (:local c) " cannot be " (:tag none) " at this point.")
         (return (& private-namespace c))))
      (production :nonexpression-attribute (static) nonexpression-attribute-static
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return (new compound-attribute (list-set-of namespace) false false static none false false)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    
    (%heading 2 "Use Directive")
    (rule :use-directive ((validate (-> (context environment) context)))
      (production :use-directive (use namespace :paren-list-expression) use-directive-normal
        ((validate cxt env)
         ((validate :paren-list-expression) cxt env)
         (const values (vector object) ((eval-as-list :paren-list-expression) env compile))
         (var namespaces (list-set namespace) (list-set-of namespace))
         (for-each values v
           (rwhen (or (not-in v namespace :narrow-false) (set-in v namespaces))
             (throw bad-value-error))
           (<- namespaces (set+ namespaces (list-set v))))
         (return (set-field cxt open-namespaces (set+ (& open-namespaces cxt) namespaces))))))
    (%print-actions ("Validation" validate))
    
    
    (%heading 2 "Import Directive")
    (production :import-directive (import :import-binding :includes-excludes) import-directive-import)
    (production :import-directive (import :import-binding \, namespace :paren-list-expression :includes-excludes)
                import-directive-import-namespaces)
    
    (production :import-binding (:import-source) import-binding-import-source)
    (production :import-binding (:identifier = :import-source) import-binding-named-import-source)
    
    (production :import-source ($string) import-source-string)
    (production :import-source (:package-name) import-source-package-name)
    
    
    (production :includes-excludes () includes-excludes-none)
    (production :includes-excludes (\, exclude \( :name-patterns \)) includes-excludes-exclude-list)
    (production :includes-excludes (\, include \( :name-patterns \)) includes-excludes-include-list)
    
    (production :name-patterns () name-patterns-empty)
    (production :name-patterns (:name-pattern-list) name-patterns-name-pattern-list)
    
    (production :name-pattern-list (:qualified-identifier) name-pattern-list-one)
    (production :name-pattern-list (:name-pattern-list \, :qualified-identifier) name-pattern-list-more)
    
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
      (production :pragma-argument (- $number) pragma-argument-negative-number (value (general-number-negate (value $number))))
      (production :pragma-argument (- $negated-min-long) pragma-argument-min-long (value (new long (neg (expt 2 63)))))
      (production :pragma-argument ($string) pragma-argument-string (value (value $string))))
    (%print-actions ("Validation" validate))
    
    (define (process-pragma (cxt context) (name string) (value object) (optional boolean)) context
      (when (= name "strict" string)
        (rwhen (in value (tag true undefined) :narrow-false)
          (return (set-field cxt strict true)))
        (rwhen (in value (tag false))
          (return (set-field cxt strict false))))
      (when (= name "ecmascript" string)
        (rwhen (set-in value (list-set-of object undefined 4.0))
          (return cxt))
        (rwhen (set-in value (list-set-of object 1.0 2.0 3.0))
          (// "An implementation may optionally modify " (:local cxt) " to disable features not available in ECMAScript Edition " (:local value)
              " other than subsequent pragmas.")
          (return cxt)))
      (if optional
        (return cxt)
        (throw bad-value-error)))
    
    
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
        ((validate cxt env attr)
         (const immutable boolean (immutable :variable-definition-kind))
         ((validate :variable-binding-list) cxt env attr immutable))
        ((eval env d)
         (const immutable boolean (immutable :variable-definition-kind))
         ((eval :variable-binding-list) env immutable)
         (return d))))
    
    (rule :variable-definition-kind ((immutable boolean))
      (production :variable-definition-kind (var) variable-definition-kind-var (immutable false))
      (production :variable-definition-kind (const) variable-definition-kind-const (immutable true)))
    
    (rule (:variable-binding-list :beta) ((validate (-> (context environment attribute-opt-not-false boolean) void))
                                          (eval (-> (environment boolean) void)))
      (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one
        ((validate cxt env attr immutable) :forward)
        ((eval env immutable) ((eval :variable-binding) env immutable)))
      (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more
        ((validate cxt env attr immutable) :forward)
        ((eval env immutable)
         ((eval :variable-binding-list) env immutable)
         ((eval :variable-binding) env immutable))))
    
    (deftag hoisted)
    (deftag instance)
    (rule (:variable-binding :beta) ((kind (writable-cell (tag hoisted static instance))) (multiname (writable-cell multiname))
                                     (pre-eval (-> (environment (union variable instance-variable) overridden-member overridden-member) void))
                                     (validate (-> (context environment attribute-opt-not-false boolean) void))
                                     (eval (-> (environment boolean) void)))
      (production (:variable-binding :beta) ((:typed-identifier :beta) (:variable-initialisation :beta)) variable-binding-full
        ((pre-eval env v overridden-read overridden-write)
         (case v
           (:narrow variable
             (const type class (get-variable-type v compile))
             (const value variable-value (& value v))
             (when (in value (-> () object) :narrow-true)
               (&= value v inaccessible)
               (catch ((const new-value object (value))
                       (const coerced-value object (assignment-conversion new-value type))
                       (&= value v coerced-value))
                 (x)
                 (rwhen (not-in x (tag compile-expression-error))
                   (throw x))
                 (// "If a " (:tag compile-expression-error) " occurred, then the initialiser is not a compile-time constant expression. "
                     "In this case, ignore the error and leave the value of the variable " (:tag inaccessible) " until it is defined at run time."))))
           (:narrow instance-variable
             (var t class-opt ((eval :typed-identifier) env))
             (when (in t (tag none))
               (cond
                ((not-in overridden-read (tag none potential-conflict) :narrow-true)
                 (assert (not-in overridden-read instance-method :narrow-true) "Note that " (:global define-instance-member)
                         " already ensured that " (:assertion) ".")
                 (<- t (&opt type overridden-read)))
                ((not-in overridden-write (tag none potential-conflict) :narrow-true)
                 (assert (not-in overridden-write instance-method :narrow-true) "Note that " (:global define-instance-member)
                         " already ensured that " (:assertion) ".")
                 (<- t (&opt type overridden-write)))
                (nil
                 (<- t object-class))))
             (&const= type v (assert-not-in t (tag none))))))
        
        ((validate cxt env attr immutable)
         ((validate :typed-identifier) cxt env)
         ((validate :variable-initialisation) cxt env)
         (const name string (name :typed-identifier))
         (cond
          ((and (not (& strict cxt)) (in (get-regional-frame env) (union global parameter-frame))
                (not immutable) (in attr (tag none)) (not (type-present :typed-identifier)))
           (action<- (kind :variable-binding 0) hoisted)
           (const qname qualified-name (new qualified-name public-namespace name))
           (action<- (multiname :variable-binding 0) (list-set qname))
           (define-hoisted-var env name))
          (nil
           (const a compound-attribute (to-compound-attribute attr))
           (rwhen (or (& dynamic a) (& prototype a))
             (throw definition-error))
           (var member-mod member-modifier (& member-mod a))
           (if (in (nth env 0) class)
             (when (in member-mod (tag none))
               (<- member-mod final))
             (rwhen (not-in member-mod (tag none))
               (throw definition-error)))
           (var v (union variable instance-variable))
           (var overridden-read overridden-member none)
           (var overridden-write overridden-member none)
           (case member-mod
             (:select (tag none static)
               (function (eval-type) class
                 (const type class-opt ((eval :typed-identifier) env))
                 (rwhen (in type (tag none) :narrow-false)
                   (return object-class))
                 (return type))
               (function (eval-initialiser) object
                 (const value object-opt ((eval :variable-initialisation) env compile))
                 (rwhen (in value (tag none) :narrow-false)
                   (throw compile-expression-error))
                 (return value))
               (var initial-value variable-value inaccessible)
               (when immutable
                 (<- initial-value eval-initialiser))
               (<- v (new variable eval-type initial-value immutable))
               (const multiname multiname (define-static-member env name (& namespaces a) (& override-mod a) (& explicit a) read-write (assert-in v variable)))
               (action<- (multiname :variable-binding 0) multiname)
               (action<- (kind :variable-binding 0) static))
             (:narrow (tag virtual final)
               (const c class (assert-in (nth env 0) class))
               (function (eval-initial-value) object-opt
                 (return ((eval :variable-initialisation) env run)))
               (<- v (new instance-variable :uninit eval-initial-value immutable (in member-mod (tag final))))
               (const os override-status-pair (define-instance-member c cxt name (& namespaces a) (& override-mod a) (& explicit a)
                                                read-write (assert-in v instance-variable)))
               (<- overridden-read (& overridden-member (& read-status os)))
               (<- overridden-write (& overridden-member (& write-status os)))
               (action<- (kind :variable-binding 0) instance))
             (:select (tag constructor)
               (throw definition-error)))
           (// "The following sets up " (:action pre-eval) " to be called during the pre-evaluation pass.")
           (function (pre-evaluate) void
             ((pre-eval :variable-binding 0) env v overridden-read overridden-write))
           (<- pre-evaluators (append pre-evaluators (vector pre-evaluate))))))
        
        ((eval env immutable)
         (case (kind :variable-binding 0)
           (:select (tag hoisted)
             (const value object-opt ((eval :variable-initialisation) env run))
             (when (not-in value (tag none) :narrow-true)
               (lexical-write env (multiname :variable-binding 0) value false run)))
           (:select (tag static)
             (const local-frame frame (nth env 0))
             (const members (list-set static-member) (map (& static-write-bindings local-frame) b (& content b) (set-in (& qname b) (multiname :variable-binding 0))))
             (// "Note that the " (:local members) " set consists of exactly one " (:type variable) " element because " (:local local-frame)
                 " was constructed with that " (:type variable) " inside " (:action validate) ".")
             (const v variable (assert-in (unique-elt-of members) variable))
             (when (in (& value v) (tag inaccessible))
               (const value object-opt ((eval :variable-initialisation) env run))
               (const type class (get-variable-type v run))
               (var coerced-value object-u)
               (cond
                ((not-in value (tag none) :narrow-true) (<- coerced-value (assignment-conversion value type)))
                (immutable (<- coerced-value uninitialised))
                (nil (<- coerced-value (assignment-conversion undefined type))))
               (&= value v coerced-value)))
           (:select (tag instance))))))
    
    (rule (:variable-initialisation :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) object-opt)))
      (production (:variable-initialisation :beta) () variable-initialisation-none
        ((validate cxt env) :forward)
        ((eval (env :unused) (phase :unused)) (return none)))
      (production (:variable-initialisation :beta) (= (:variable-initialiser :beta)) variable-initialisation-variable-initialiser
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :variable-initialiser) env phase)))))
    
    (rule (:variable-initialiser :beta) ((validate (-> (context environment) void)) (eval (-> (environment phase) object)))
      (production (:variable-initialiser :beta) ((:assignment-expression :beta)) variable-initialiser-assignment-expression
        ((validate cxt env) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :assignment-expression) env phase))
         (return (read-reference r phase))))
      (production (:variable-initialiser :beta) (:nonexpression-attribute) variable-initialiser-nonexpression-attribute
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :nonexpression-attribute) env phase))))
      (production (:variable-initialiser :beta) (:attribute-combination) variable-initialiser-attribute-combination
        ((validate cxt env) :forward)
        ((eval env phase) (return ((eval :attribute-combination) env phase)))))
    
    
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
    (%print-actions ("Validation" name type-present immutable kind multiname validate) ("Pre-Evaluation" pre-eval) ("Evaluation" eval))
    
    
    (%heading 2 "Simple Variable Definition")
    (%text :syntax "A " (:grammar-symbol :simple-variable-definition) " represents the subset of " (:grammar-symbol :variable-definition)
           " expansions that may be used when the variable definition is used as a " (:grammar-symbol (:substatement :omega))
           " instead of a " (:grammar-symbol (:directive :omega_2)) " in non-strict mode. "
           "In strict mode variable definitions may not be used as substatements.")
    (rule :simple-variable-definition ((validate (-> (context environment) void)) (eval (-> (environment object) object)))
      (production :simple-variable-definition (var :untyped-variable-binding-list) simple-variable-definition-definition
        ((validate cxt env)
         (rwhen (or (& strict cxt) (not-in (get-regional-frame env) (union global parameter-frame)))
           (throw syntax-error))
         ((validate :untyped-variable-binding-list) cxt env))
        ((eval env d)
         ((eval :untyped-variable-binding-list) env)
         (return d))))
    
    (rule :untyped-variable-binding-list ((validate (-> (context environment) void)) (eval (-> (environment) void)))
      (production :untyped-variable-binding-list (:untyped-variable-binding) untyped-variable-binding-list-one
        ((validate cxt env) :forward)
        ((eval env) ((eval :untyped-variable-binding) env)))
      (production :untyped-variable-binding-list (:untyped-variable-binding-list \, :untyped-variable-binding) untyped-variable-binding-list-more
        ((validate cxt env) :forward)
        ((eval env)
         ((eval :untyped-variable-binding-list) env)
         ((eval :untyped-variable-binding) env))))
    
    (rule :untyped-variable-binding ((validate (-> (context environment) void)) (eval (-> (environment) void)))
      (production :untyped-variable-binding (:identifier (:variable-initialisation allow-in)) untyped-variable-binding-full
        ((validate cxt env)
         ((validate :variable-initialisation) cxt env)
         (define-hoisted-var env (name :identifier)))
        ((eval env)
         (const value object-opt ((eval :variable-initialisation) env run))
         (when (not-in value (tag none) :narrow-true)
           (const qname qualified-name (new qualified-name public-namespace (name :identifier)))
           (lexical-write env (list-set qname) value false run)))))
    (%print-actions ("Validation" validate) ("Evaluation" eval))
    
    
    (%heading 2 "Function Definition")
    (rule :function-definition ((pre-eval (-> (context environment parameter-frame boolean) void))
                                (validate (-> (context environment plurality attribute-opt-not-false) void)))
      (production :function-definition (function :function-name :function-signature :block) function-definition-definition
        ((pre-eval cxt compile-env compile-frame unchecked)
         (&const= signature compile-frame ((pre-eval :function-signature) cxt compile-env unchecked)))
        
        ((validate cxt env pl attr)
         (const name string (name :function-name))
         (const kind function-kind (kind :function-name))
         (const a compound-attribute (to-compound-attribute attr))
         (rwhen (& dynamic a)
           (throw definition-error))
         (const unchecked boolean (and (not (& strict cxt)) (not-in (nth env 0) class) (in kind (tag normal)) (untyped :function-signature)))
         (const prototype boolean (or unchecked (& prototype a)))
         (var member-mod member-modifier (& member-mod a))
         (if (in (nth env 0) class)
           (when (in member-mod (tag none))
             (<- member-mod virtual))
           (rwhen (not-in member-mod (tag none))
             (throw definition-error)))
         (rwhen (and prototype (or (not-in kind (tag normal)) (in member-mod (tag constructor))))
           (throw definition-error))
         (var compile-this (tag none inaccessible) none)
         (when (or prototype (in member-mod (tag constructor virtual final)))
           (<- compile-this inaccessible))
         (const compile-frame parameter-frame (new parameter-frame (list-set-of static-binding) (list-set-of static-binding) plural compile-this prototype :uninit))
         (const compile-env environment (cons compile-frame env))
         ((validate :function-signature) cxt compile-env)
         ((validate :block) cxt compile-env (new jump-targets (list-set-of label) (list-set-of label)) plural)
         (cond
          ((and unchecked (in (nth env 0) (union global parameter-frame)) (in attr (tag none)))
           (const v hoisted-var (new hoisted-var undefined true))
           (define-hoisted-var env name)
           (todo))
          (nil
           (case member-mod
             (:select (tag none static)
               (function (call (this object) (args argument-list) (runtime-env environment) (phase phase)) object
                 (rwhen (in phase (tag compile))
                   (throw compile-expression-error))
                 (var runtime-this object-opt)
                 (case compile-this
                   (:select (tag none) (<- runtime-this none))
                   (:select (tag inaccessible)
                     (<- runtime-this this)
                     (const g (union package global) (get-package-or-global-frame runtime-env))
                     (when (and prototype (in runtime-this (tag null undefined)) (in g global :narrow-true))
                       (<- runtime-this g))))
                 (const runtime-frame parameter-frame
                   (new parameter-frame (list-set-of static-binding) (list-set-of static-binding) singular runtime-this prototype (&opt signature compile-frame)))
                 (instantiate-frame compile-frame runtime-frame (cons runtime-frame runtime-env))
                 (assign-arguments runtime-frame (&opt signature compile-frame) unchecked args)
                 (catch ((exec ((eval :block) (cons runtime-frame runtime-env) undefined))
                         (throw (new returned-value undefined)))
                   (x) (cond
                        ((in x returned-value :narrow-true)
                         (return (& value x)))
                        (nil (throw x)))))
               (function (construct (args argument-list) (runtime-env environment) (phase phase)) object
                 (todo))
               (var f (union instance open-instance))
               (cond
                ((in kind (tag get set))
                 (todo))
                (prototype
                 (todo))
                (nil
                 (function (instantiate (runtime-env environment)) non-alias-instance
                   (return (new fixed-instance function-class call bad-construct env "Function" (list-set-of slot))))
                 (<- f (new open-instance instantiate none))))
               (when (in pl (tag singular))
                 (<- f (instantiate-open-instance (assert-in f open-instance) env)))
               (const v variable (new variable function-class f true))
               (exec (define-static-member env name (& namespaces a) (& override-mod a) (& explicit a) read-write v)))
             (:narrow (tag virtual final)
               (todo))
             (:select (tag constructor)
               (todo)))))
         (// "The following sets up " (:action pre-eval) " to be called during the pre-evaluation pass.")
         (function (pre-evaluate) void
           ((pre-eval :function-definition 0) cxt compile-env compile-frame unchecked))
         (<- pre-evaluators (append pre-evaluators (vector pre-evaluate))))))
    
    (rule :function-name ((kind function-kind) (name string))
      (production :function-name (:identifier) function-name-function
        (kind normal)
        (name (name :identifier)))
      (production :function-name (get :no-line-break :identifier) function-name-getter
        (kind get)
        (name (name :identifier)))
      (production :function-name (set :no-line-break :identifier) function-name-setter
        (kind set)
        (name (name :identifier))))
    (%print-actions ("Validation" kind name signature validate) ("Pre-Evaluation" pre-eval))
    
    (define (assign-arguments (runtime-frame parameter-frame) (sig signature) (unchecked boolean) (args argument-list)) void
      (todo))
    
    
    (rule :function-signature ((untyped boolean)
                               (validate (-> (context environment) void))
                               (pre-eval (-> (context environment boolean) signature)))
      (production :function-signature (:parameter-signature :result-signature) function-signature-parameter-and-result-signatures
        (untyped false)
        ((validate (cxt :unused) (env :unused)) (todo))
        ((pre-eval (cxt :unused) (env :unused) (unchecked :unused)) (todo))))
    
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
    (%print-actions ("Validation" validate) ("Pre-Evaluation" pre-eval))
    
    
    (%heading 2 "Class Definition")
    (rule :class-definition ((class (writable-cell class)) (validate (-> (context environment plurality attribute-opt-not-false) void))
                             (eval (-> (environment object) object)))
      (production :class-definition (class :identifier :inheritance :block) class-definition-definition
        ((validate cxt env pl attr)
         (rwhen (not-in pl (tag singular))
           (throw syntax-error))
         (const superclass class ((validate :inheritance) cxt env))
         (var a compound-attribute (to-compound-attribute attr))
         (rwhen (or (not (& complete superclass)) (& final superclass))
           (throw definition-error))
         (function (call (this object :unused) (args argument-list :unused) (phase phase :unused)) object
           (todo))
         (function (construct (args argument-list :unused) (phase phase :unused)) object
           (todo))
         (var prototype object null)
         (when (& prototype a)
           (todo))
         (var final boolean)
         (case (& member-mod a)
           (:select (tag none) (<- final false))
           (:select (tag static)
             (rwhen (not-in (nth env 0) class)
               (throw definition-error))
             (<- final false))
           (:select (tag final) (<- final true))
           (:select (tag constructor virtual) (throw definition-error)))
         (const private-namespace namespace (new namespace "private"))
         (const dynamic boolean (or (& dynamic a) (& dynamic superclass)))
         (const c class (new class (list-set-of static-binding) (list-set-of static-binding) (list-set-of instance-binding) (list-set-of instance-binding)
                             (vector-of instance-variable) false superclass prototype private-namespace dynamic true final call construct))
         (action<- (class :class-definition 0) c)
         (const v variable (new variable class-class c true))
         (exec (define-static-member env (name :identifier) (& namespaces a) (& override-mod a) (& explicit a) read-write v))
         ((validate-using-frame :block) cxt env (new jump-targets (list-set-of label) (list-set-of label)) pl c)
         (&= complete c true))
        ((eval env d)
         (const c class (class :class-definition 0))
         (return ((eval-using-frame :block) env c d)))))
    
    
    (rule :inheritance ((validate (-> (context environment) class)))
      (production :inheritance () inheritance-none
        ((validate (cxt :unused) (env :unused)) (return object-class)))
      (production :inheritance (extends (:type-expression allow-in)) inheritance-extends
        ((validate cxt env)
         ((validate :type-expression) cxt env)
         (return ((eval :type-expression) env))))
      #|(production :inheritance (implements :type-expression-list) inheritance-implements
        ((validate (cxt :unused) (env :unused)) (return object-class)))
      (production :inheritance (extends (:type-expression allow-in) implements :type-expression-list) inheritance-extends-implements
        ((validate (cxt :unused) (env :unused)) (return object-class)))|#)
    (%print-actions ("Validation" class validate) ("Evaluation" eval))
    
    
    ;(%heading 2 "Interface Definition")
    ;(production (:interface-definition :omega_2) (interface :identifier :extends-list :block) interface-definition-definition)
    ;(production (:interface-definition :omega_2) (interface :identifier (:semicolon :omega_2)) interface-definition-declaration)
    ;***** Clear break and continue inside validate
    
    ;(production :extends-list () extends-list-none)
    ;(production :extends-list (extends :type-expression-list) extends-list-one)
    
    ;(production :type-expression-list ((:type-expression allow-in)) type-expression-list-one)
    ;(production :type-expression-list (:type-expression-list \, (:type-expression allow-in)) type-expression-list-more)
    
    
    (%heading 2 "Namespace Definition")
    (rule :namespace-definition ((validate (-> (context environment plurality attribute-opt-not-false) void)))
      (production :namespace-definition (namespace :identifier) namespace-definition-normal
        ((validate (cxt :unused) env pl attr)
         (rwhen (not-in pl (tag singular))
           (throw syntax-error))
         (var a compound-attribute (to-compound-attribute attr))
         (rwhen (or (& dynamic a) (& prototype a))
           (throw definition-error))
         (rwhen (not (or (in (& member-mod a) (tag none)) (and (in (& member-mod a) (tag static)) (in (nth env 0) class))))
           (throw definition-error))
         (const name string (name :identifier))
         (const ns namespace (new namespace name))
         (const v variable (new variable namespace-class ns true))
         (exec (define-static-member env name (& namespaces a) (& override-mod a) (& explicit a) read-write v)))))
    (%print-actions ("Validation" validate))
    
    
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
          (const saved-pre-evaluators (vector (-> () void)) pre-evaluators)
          (<- pre-evaluators (vector-of (-> () void)))
          (exec ((validate :directives) initial-context initial-environment (new jump-targets (list-set-of label) (list-set-of label)) singular none))
          (for-each pre-evaluators v (v))
          (<- pre-evaluators saved-pre-evaluators)
          (return ((eval :directives) initial-environment undefined))))))
    (%print-actions ("Evaluation" eval-program))
    
    
    
    (%heading (1 :semantics) "Predefined Identifiers")
    
    
    (%heading (1 :semantics) "Built-in Classes")
    (define (make-built-in-class (superclass class-opt) (dynamic boolean) (allow-null boolean) (final boolean)) class
      (function (call (this object :unused) (args argument-list :unused) (phase phase :unused)) object
        (todo))
      (function (construct (args argument-list :unused) (phase phase :unused)) object
        (todo))
      (const private-namespace namespace (new namespace "private"))
      (return (new class (list-set-of static-binding) (list-set-of static-binding) (list-set-of instance-binding) (list-set-of instance-binding)
                   (vector-of instance-variable) true superclass null private-namespace dynamic allow-null final call construct)))
    
    (define object-class class (make-built-in-class none false true false))
    (define undefined-class class (make-built-in-class object-class false false true))
    (define null-class class (make-built-in-class object-class false true true))
    (define boolean-class class (make-built-in-class object-class false false true))
    (define general-number-class class (make-built-in-class object-class false false false))
    (define long-class class (make-built-in-class general-number-class false false true))
    (define u-long-class class (make-built-in-class general-number-class false false true))
    (define float-class class (make-built-in-class general-number-class false false true))
    (define number-class class (make-built-in-class general-number-class false false true))
    (define character-class class (make-built-in-class object-class false false true))
    (define string-class class (make-built-in-class object-class false true true))
    (define namespace-class class (make-built-in-class object-class false true true))
    (define attribute-class class (make-built-in-class object-class false true true))
    (define class-class class (make-built-in-class object-class false true true))
    (define function-class class (make-built-in-class object-class false true true))
    (define prototype-class class (make-built-in-class object-class true true true))
    (define package-class class (make-built-in-class object-class true true true))
    
    
    (define object-prototype prototype (new prototype none (list-set-of dynamic-property))) ;***** Add some properties here
    
    (%heading (1 :semantics) "Built-in Functions")
    (%heading (1 :semantics) "Built-in Attributes")
    (%heading (1 :semantics) "Built-in Namespaces")
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
