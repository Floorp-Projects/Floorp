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
    (deftag reject)
    
    (deftype object (union undefined null boolean long u-long float32 float64 character string namespace compound-attribute
                           class simple-instance method-closure date reg-exp package global-object))
    (deftype primitive-object (union undefined null boolean long u-long float32 float64 character string))
    
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
    ;(deftype qualified-name-opt (union qualified-name (tag none)))
    
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
      (local-bindings (list-set local-binding) :var)
      (parent class-opt)
      (instance-members (list-set instance-member) :var)
      (instance-init-order (vector instance-variable) :var)
      (complete boolean :var)
      (super class-opt)
      (prototype object)
      (typeof-string string)
      (private-namespace namespace)
      (dynamic boolean)
      (final boolean)
      (call (-> (object (vector object) phase) object))
      (construct (-> ((vector object) phase) object))
      (is-instance-of (-> (object) boolean) :opt-const)
      (implicit-coerce (-> (object boolean) object) :opt-const)
      (default-value object))
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

    #|
    (%text :comment "Return " (:tag true) " if " (:local c) " is an ancestor of " (:local d) " other than " (:local d) " itself.")
    (define (is-proper-ancestor (c class) (d class)) boolean
      (return (and (is-ancestor c d) (/= c d class))))
    |#
    
    
    (%heading (3 :semantics) "Simple Instances")
    (defrecord simple-instance
      (local-bindings (list-set local-binding) :var)
      (parent object-opt)
      (sealed boolean)
      (type class)
      (slots (list-set slot) :var)
      (call (union (-> (object (vector object) environment phase) object) (tag none)))
      (construct (union (-> ((vector object) environment phase) object) (tag none)))
      (to-class class-opt)
      (env environment-opt))
    
    
    (%heading (4 :semantics) "Slots")
    (defrecord slot
      (id instance-variable)
      (value object-u :var))
    
    
    (%heading (3 :semantics) "Uninstantiated Functions")
    (defrecord uninstantiated-function
      (type class)
      (default-slots (list-set slot))
      (build-prototype boolean)
      (call (union (-> (object (vector object) environment phase) object) (tag none)))
      (construct (union (-> ((vector object) environment phase) object) (tag none)))
      (instantiations (list-set simple-instance) :var))
    
    
    (%heading (3 :semantics) "Method Closures")
    (deftuple method-closure
      (this object)
      (method instance-method))
    
    
    (%heading (3 :semantics) "Dates")
    (defrecord date
      (local-bindings (list-set local-binding) :var)
      (parent object-opt)
      (sealed boolean)
      (time-value integer))
    
    
    (%heading (3 :semantics) "Regular Expressions")
    (defrecord reg-exp
      (local-bindings (list-set local-binding) :var)
      (parent object-opt)
      (sealed boolean)
      (source string)
      (last-index integer)
      (global boolean)
      (ignore-case boolean)
      (multiline boolean))
    
   
    (%heading (3 :semantics) "Packages")
    (defrecord package
      (local-bindings (list-set local-binding) :var)
      (internal-namespace namespace))
    
    
    (%heading (3 :semantics) "Global Objects")
    (defrecord global-object
      (local-bindings (list-set local-binding) :var)
      (parent object-opt)
      (sealed boolean)
      (internal-namespace namespace))
    
    
    (%heading (2 :semantics) "Objects with Limits")
    (%text :comment (:label limited-instance instance) " must be an instance of " (:label limited-instance limit) " or one of "
           (:label limited-instance limit) :apostrophe "s descendants.")
    (deftuple limited-instance
      (instance object)
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
      (args (vector object)))
    
    (deftype reference (union lexical-reference dot-reference bracket-reference))
    (deftype obj-or-ref (union object reference))
    
    
    (%heading (2 :semantics) "Function Support")
    (deftag normal)
    (deftag get)
    (deftag set)
    (deftype function-kind (tag normal get set))
    
    
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
           ". The next-to-last frame is always a " (:type package) " or " (:type global-object) ".")
    (deftype environment (vector frame))
    (deftype environment-i (union environment (tag inaccessible)))
    (deftype environment-opt (union environment (tag none)))
    (deftype frame (union system-frame global-object package parameter-frame class block-frame))
    
    (defrecord system-frame
      (local-bindings (list-set local-binding) :var))
    
    (defrecord parameter-frame
      (local-bindings (list-set local-binding) :var)
      (plurality plurality)
      (this object-i-opt)
      (unchecked boolean)
      (prototype boolean)
      (parameters (vector parameter) :opt-var)
      (rest (union variable (tag none)) :opt-var)
      (return-type class :opt-const))
     
    (deftuple parameter
      (var (union dynamic-var variable))
      (default object-opt))
    
    (defrecord block-frame
      (local-bindings (list-set local-binding) :var)
      (plurality plurality))
    
    (define initial-environment environment (vector-of frame (new system-frame (list-set-of local-binding))))
    
    
    (%heading (3 :semantics) "Members")
    (deftag read)
    (deftag write)
    (deftag read-write)
    (deftype access (tag read write))
    (deftype access-set (tag read write read-write))
    
    
    (deftuple local-binding 
      (qname qualified-name)
      (accesses access-set)
      (content local-member)
      (explicit boolean))
    
    
    (deftag forbidden)
    (deftype local-member (union (tag forbidden) dynamic-var variable constructor-method getter setter))
    (deftype local-member-opt (union local-member (tag none)))
   
    (deftype variable-type (union class (tag inaccessible) (-> () class)))
    (deftype variable-value (union object (tag inaccessible uninitialised) uninstantiated-function (-> () object)))
    (defrecord variable
      (type variable-type :var)
      (value variable-value :var)
      (immutable boolean))
    
    (defrecord dynamic-var
      (value (union object uninstantiated-function) :var)
      (sealed boolean :var))
    
    (defrecord constructor-method
      (code object))  ;Constructor code
    
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
      (multiname multiname :opt-const)
      (final boolean)
      (type class :opt-const)
      (eval-initial-value (-> () object-opt))
      (immutable boolean))
    (deftype instance-variable-opt (union instance-variable (tag none)))
    
    (defrecord instance-method
      (multiname multiname :opt-const)
      (final boolean)
      (signature parameter-frame)
      (call (-> (object (vector object) phase) object)))
    (deftype instance-method-opt (union instance-method (tag none)))
    
    (defrecord instance-getter
      (multiname multiname :opt-const)
      (final boolean)
      (type class :opt-const)
      (call (-> (object environment phase) object))
      (env environment))
    
    (defrecord instance-setter
      (multiname multiname :opt-const)
      (final boolean)
      (type class :opt-const)
      (call (-> (object object environment phase) void))
      (env environment))
    
    
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
        (assert (cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 64) 1)))
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
        (assert (cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 64) 1)))
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
        (:narrow simple-instance (return (& type o)))
        (:select method-closure (return function-class))
        (:select date (return date-class))
        (:select reg-exp (return reg-exp-class))
        (:select (union package global-object) (return package-class))))
    
    
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
        (:select (union character namespace compound-attribute class simple-instance method-closure date reg-exp package global-object) (return true))))
    
    
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
        (:select (union namespace compound-attribute class method-closure package global-object) (throw bad-value-error))
        (:select simple-instance (todo))
        (:select date (todo))
        (:select reg-exp (todo))))
    
    
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
        (:select simple-instance (todo))
        (:select date (todo))
        (:select reg-exp (todo))
        (:select (union package global-object) (todo))))
    
    
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
                (:expr boolean (= (real-to-float32 (rat* s (expt 10 (- n k)))) x float32)) ", and " (:local k) " is as small as possible.")
            (note (:local k) " is the number of digits in the decimal representation of " (:local s) ", " (:local s)
                  " is not divisible by 10, and the least significant digit of " (:local s)
                  " is not necessarily uniquely determined by the above criteria.")
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
                (:expr boolean (= (real-to-float64 (rat* s (expt 10 (- n k)))) x float64)) ", and " (:local k) " is as small as possible.")
            (note (:local k) " is the number of digits in the decimal representation of " (:local s) ", that " (:local s)
                  " is not divisible by 10, and that the least significant digit of " (:local s)
                  " is not necessarily uniquely determined by the above criteria.")
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
    
    
    (%heading (3 :semantics) (:global to-qualified-name nil))
    (%text :comment (:global-call to-qualified-name o phase) " coerces an object " (:local o) " to a qualified name. If "
           (:local phase) " is " (:tag compile) ", only compile-time conversions are permitted.")
    (define (to-qualified-name (o object) (phase phase)) qualified-name
      (return (new qualified-name public-namespace (to-string o phase))))
    
    
    (%heading (3 :semantics) (:global to-primitive nil))
    (define (to-primitive (o object) (hint object :unused) (phase phase)) primitive-object
      (case o
        (:narrow primitive-object (return o))
        (:select (union namespace compound-attribute class simple-instance method-closure reg-exp package global-object) (return (to-string o phase)))
        (:narrow date (todo))))
    
    
    (%heading (3 :semantics) (:global to-class nil))
    (define (to-class (o object)) class
      (case o
        (:narrow class (return o))
        (:narrow simple-instance
          (const c class-opt (& to-class o))
          (if (not-in c (tag none) :narrow-true)
            (return c)
            (throw bad-value-error)))
        (:select (union undefined null boolean long u-long float32 float64 character string namespace compound-attribute
                        method-closure date reg-exp package global-object)
          (throw bad-value-error))))
    
    
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
        (note "At this point both " (:local a) " and " (:local b) " are compound attributes. Ensure that they have no conflicting contents.")
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
    
    (define (bracket-read (a obj-optional-limit) (args (vector object)) (phase phase)) object
      (rwhen (/= (length args) 1)
        (throw argument-mismatch-error))
      (const name string (to-string (nth args 0) phase))
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
    
    (define (bracket-write (a obj-optional-limit) (args (vector object)) (new-value object) (phase phase)) (tag none ok)
      (rwhen (in phase (tag compile) :narrow-false)
        (throw compile-expression-error))
      (rwhen (/= (length args) 1)
        (throw argument-mismatch-error))
      (const name string (to-string (nth args 0) phase))
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
    
    (define (bracket-delete (a obj-optional-limit) (args (vector object)) (phase phase)) boolean-opt
      (rwhen (in phase (tag compile) :narrow-false)
        (throw compile-expression-error))
      (rwhen (/= (length args) 1)
        (throw argument-mismatch-error))
      (const name string (to-string (nth args 0) phase))
      (return (delete-property a (list-set (new qualified-name public-namespace name)) property-lookup phase)))
    
    
    (%heading (2 :semantics) "Slots")
    
    (define (find-slot (o object) (id instance-variable)) slot
      (assert (in o simple-instance :narrow-true)
        (:local o) " must be a " (:type simple-instance) ".")
      (const matching-slots (list-set slot)
        (map (& slots o) s s (= (& id s) id instance-variable)))
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
    
    
    (define (get-package-or-global-frame (env environment)) (union package global-object)
      (const g frame (nth env (- (length env) 2)))
      (assert (in g (union package global-object) :narrow-true)
        "The penultimate frame " (:local g) " is always a " (:type package) " or " (:type global-object) ".")
      (return g))
    
    
    (%heading (3 :semantics) "Access Utilities")
    (define (accesses-overlap (accesses1 access-set) (accesses2 access-set)) boolean
      (return (or (= accesses1 accesses2 access-set)
                  (in accesses1 (tag read-write))
                  (in accesses2 (tag read-write)))))
    
    
    (%heading (3 :semantics) "Adding Local Definitions")
    (define (define-local-member (env environment) (id string) (namespaces (list-set namespace)) (override-mod override-modifier) (explicit boolean)
              (accesses access-set) (m local-member))
            multiname
      (const local-frame frame (nth env 0))
      (rwhen (or (not-in override-mod (tag none)) (and explicit (not-in local-frame package)))
        (throw definition-error))
      (var namespaces2 (list-set namespace) namespaces)
      (when (empty namespaces2)
        (<- namespaces2 (list-set public-namespace)))
      (const multiname multiname (map namespaces2 ns (new qualified-name ns id)))
      (const regional-env (vector frame) (get-regional-environment env))
      (rwhen (some (& local-bindings local-frame) b (and (set-in (& qname b) multiname) (accesses-overlap (& accesses b) accesses)))
        (throw definition-error))
      (for-each (subseq regional-env 1) frame
        (rwhen (some (& local-bindings frame) b (and (set-in (& qname b) multiname) (accesses-overlap (& accesses b) accesses)
                                                     (not-in (& content b) (tag forbidden))))
          (throw definition-error)))
      (const new-bindings (list-set local-binding) (map multiname qname (new local-binding qname accesses m explicit)))
      (&= local-bindings local-frame (set+ (& local-bindings local-frame) new-bindings))
      (note "Mark the bindings of " (:local multiname) " as " (:tag forbidden)
            " in all non-innermost frames in the current region if they haven" :apostrophe "t been marked as such already.")
      (const new-forbidden-bindings (list-set local-binding) (map multiname qname (new local-binding qname accesses forbidden true)))
      (for-each (subseq regional-env 1) frame
        (&= local-bindings frame (set+ (& local-bindings frame) new-forbidden-bindings)))
      (return multiname))
      
    
    (%text :comment (:global-call define-hoisted-var env id initial-value) " defines a hoisted variable with the name " (:local id)
           " in the environment " (:local env) ". Hoisted variables are hoisted to the global or enclosing function scope. "
           "Multiple hoisted variables may be defined in the same scope, but they may not coexist with non-hoisted variables with the same name. "
           "A hoisted variable can be defined using either a " (:character-literal "var") " or a " (:character-literal "function") " statement. "
           "If it is defined using " (:character-literal "var") ", then " (:local initial-value) " is always " (:tag undefined)
           " (if the " (:character-literal "var") " statement has an initialiser, then the variable" :apostrophe "s value will be written later when the "
           (:character-literal "var") " statement is executed). "
           "If it is defined using " (:character-literal "function") ", then " (:local initial-value) " must be a function instance or open instance. "
           "A " (:character-literal "var") " hoisted variable may be hoisted into the " (:type parameter-frame)
           " if there is already a parameter with the same name; a " (:character-literal "function") " hoisted variable is never hoisted into the "
           (:type parameter-frame) " and will shadow a parameter with the same name for compatibility with ECMAScript Edition 3. "
           "If there are multiple " (:character-literal "function") " definitions, the initial value is the last " (:character-literal "function") " definition.")
    (define (define-hoisted-var (env environment) (id string) (initial-value (union object uninstantiated-function))) dynamic-var
      (const qname qualified-name (new qualified-name public-namespace id))
      (const regional-env (vector frame) (get-regional-environment env))
      (var regional-frame frame (nth regional-env (- (length regional-env) 1)))
      (assert (in regional-frame (union global-object parameter-frame) :narrow-true)
        (:local env) " is either the " (:type global-object) " or a " (:type parameter-frame)
        " because hoisting only occurs into global or function scope.")
      (var existing-bindings (list-set local-binding) (map (& local-bindings regional-frame) b b (= (& qname b) qname qualified-name)))
      (when (and (or (empty existing-bindings) (not-in initial-value (tag undefined)))
                 (in regional-frame parameter-frame) (>= (length regional-env) 2))
        (<- regional-frame (nth regional-env (- (length regional-env) 2)) :end-narrow)
        (<- existing-bindings (map (& local-bindings regional-frame) b b (= (& qname b) qname qualified-name))))
      (cond
       ((empty existing-bindings)
        (const v dynamic-var (new dynamic-var initial-value true))
        (&= local-bindings regional-frame (set+ (& local-bindings regional-frame) (list-set (new local-binding qname read-write v false))))
        (return v))
       ((/= (length existing-bindings) 1)
        (throw definition-error))
       (nil
        (const b local-binding (unique-elt-of existing-bindings))
        (const m local-member (& content b))
        (rwhen (or (not-in (& accesses b) (tag read-write)) (not-in m dynamic-var :narrow-false) (not (& sealed m)))
          (throw definition-error))
        (note "At this point a hoisted binding of the same " (:character-literal "var") " already exists, so there is no need to create another one. "
              "Overwrite its initial value if the new definition is a " (:character-literal "function") " definition.")
        (when (not-in initial-value (tag undefined))
          (&= value m initial-value))
        (return m))))
    
    
    (%heading (3 :semantics) "Adding Instance Definitions")
    
    (define (search-for-overrides (c class) (multiname multiname) (accesses access-set)) instance-member-opt
      (var m-base instance-member-opt none)
      (const s class-opt (& super c))
      (when (not-in s (tag none) :narrow-true)
        (for-each multiname qname
          (const m instance-member-opt (find-base-instance-member s (list-set qname) accesses))
          (cond
           ((in m-base (tag none) :narrow-false) (<- m-base m))
           ((and (not-in m (tag none) :narrow-true) (/= m m-base instance-member))
            (throw definition-error)))))
      (return m-base))
    
    
    (define (define-instance-member (c class) (cxt context) (id string) (namespaces (list-set namespace))
              (override-mod override-modifier) (explicit boolean) (m instance-member))
            instance-member-opt
      (rwhen explicit
        (throw definition-error))
      (const accesses access-set (instance-member-accesses m))
      (const requested-multiname multiname (map namespaces ns (new qualified-name ns id)))
      (const open-multiname multiname (map (& open-namespaces cxt) ns (new qualified-name ns id)))
      (var defined-multiname multiname)
      (var searched-multiname multiname)
      (cond
       ((empty requested-multiname)
        (<- defined-multiname (list-set (new qualified-name public-namespace id)))
        (<- searched-multiname open-multiname)
        (assert (set<= defined-multiname searched-multiname multiname) (:assertion) " because the " (:character-literal "public") " namespace is always open."))
       (nil
        (<- defined-multiname requested-multiname)
        (<- searched-multiname requested-multiname)))
      (const m-base instance-member-opt (search-for-overrides c searched-multiname accesses))
      (var m-overridden instance-member-opt none)
      (when (not-in m-base (tag none) :narrow-true)
        (<- m-overridden (get-derived-instance-member c m-base accesses))
        (<- defined-multiname (&opt multiname (assert-not-in m-overridden (tag none))))
        (rwhen (not (set<= requested-multiname defined-multiname multiname))
          (throw definition-error))
        (var good-kind boolean)
        (case m
          (:select instance-variable (<- good-kind (in m-overridden instance-variable)))
          (:select instance-getter (<- good-kind (in m-overridden (union instance-variable instance-getter))))
          (:select instance-setter (<- good-kind (in m-overridden (union instance-variable instance-setter))))
          (:select instance-method (<- good-kind (in m-overridden instance-method))))
        (rwhen (or (& final (assert-not-in m-overridden (tag none))) (not good-kind))
          (throw definition-error)))
      (rwhen (some (& instance-members c) m2 (and (nonempty (set* (&opt multiname m2) defined-multiname)) (accesses-overlap (instance-member-accesses m2) accesses)))
        (throw definition-error))
      (case override-mod
        (:select (tag none)
          (rwhen (or (not-in m-base (tag none)) (not-in (search-for-overrides c open-multiname accesses) (tag none)))
            (throw definition-error)))
        (:select (tag false)
          (rwhen (not-in m-base (tag none))
            (throw definition-error)))
        (:select (tag true)
          (rwhen (in m-base (tag none))
            (throw definition-error)))
        (:select (tag undefined)))
      (&const= multiname m defined-multiname)
      (&= instance-members c (set+ (& instance-members c) (list-set m)))
      (return m-overridden))
    
    
    (%heading (3 :semantics) "Instantiation")
    
    (define (instantiate-function (uf uninstantiated-function) (env environment)) simple-instance
      (var slots (list-set slot) (map (& default-slots uf) s (new slot (& id s) (& value s))))
      (var local-bindings (list-set local-binding) (list-set-of local-binding))
      (var sealed boolean)
      (var parent object-opt none)
      (cond
       ((& build-prototype uf)
        (<- sealed false)
        ;***** Set parent
        (todo))
       (nil (<- sealed true)))
      (const i simple-instance (new simple-instance local-bindings parent sealed (& type uf) slots (& call uf) (& construct uf) none env))
      (const instantiations (list-set simple-instance) (& instantiations uf))
      (when (nonempty instantiations)
        (// (:def-const i2 simple-instance) "Suppose that " (:global instantiate-function) " were to choose at its discretion some element " (:local i2) " of "
            (:local instantiations) ", assign " (:expr environment-opt (& env i2)) :nbsp :assign-10 :nbsp (:local env) ", and return " (:local i) ". "
            "If the behaviour of doing that assignment were observationally indistinguishable by the rest of the program from the behaviour of "
            "returning " (:local i) " without modifying " (:expr environment-opt (& env i2)) ", then the implementation may, but does not have to, "
            (:keyword return) :nbsp (:local i2) " now, discarding (or not even bothering to create) the value of " (:local i) ".")
        (note "The above rule allows an implementation to avoid creating a fresh closure each time a local function is instantiated if it can show that the "
              "closures would behave identically. This optimisation is not transparent to the programmer because the instantiations will be "
              (:character-literal "===") " to each other and share one set of "
              "properties (including the " (:character-literal "prototype") " property, if applicable) rather than each having its own. ECMAScript programs "
              "should not rely on this distinction."))
      (&= instantiations uf (set+ instantiations (list-set i)))
      (return i))
    
    
    (define (instantiate-member (m local-member) (env environment)) local-member
      (case m
        (:select (union (tag forbidden) constructor-method) (return m))
        (:narrow variable
          (var value variable-value (& value m))
          (when (in value uninstantiated-function :narrow-true)
            (<- value (instantiate-function value env) :end-narrow))
          (return (new variable (& type m) value (& immutable m))))
        (:narrow dynamic-var
          (var value (union object uninstantiated-function) (& value m))
          (when (in value uninstantiated-function :narrow-true)
            (<- value (instantiate-function value env) :end-narrow))
          (return (new dynamic-var value (& sealed m))))
        (:narrow getter
          (case (& env m)
            (:select environment (return m))
            (:select (tag inaccessible) (return (new getter (& type m) (& call m) env)))))
        (:narrow setter
          (case (& env m)
            (:select environment (return m))
            (:select (tag inaccessible) (return (new setter (& type m) (& call m) env)))))))
    
    
    (deftuple member-translation
      (plural-member local-member)
      (singular-member local-member))
    
    (define (instantiate-block-frame (plural-frame block-frame) (env environment))
            block-frame
      (const singular-frame block-frame (new block-frame (list-set-of local-binding) singular))
      (const plural-members (list-set local-member)
        (map (& local-bindings plural-frame) b (& content b)))
      (const member-translations (list-set member-translation)
        (map plural-members m (new member-translation m (instantiate-member m (cons singular-frame env)))))
      (function (translate-member (m local-member)) local-member
        (const mi member-translation (unique-elt-of member-translations mi (= (& plural-member mi) m local-member)))
        (return (& singular-member mi)))
      (&= local-bindings singular-frame (map (& local-bindings plural-frame) b (set-field b content (translate-member (& content b)))))
      (return singular-frame))
    
    
    (define (instantiate-parameter-frame (plural-frame parameter-frame) (env environment) (singular-this object-opt))
            parameter-frame
      (const singular-frame parameter-frame (new parameter-frame (list-set-of local-binding) singular singular-this
                                                 (& unchecked plural-frame) (& prototype plural-frame)
                                                 :uninit :uninit
                                                 (&opt return-type plural-frame)))
      (const plural-members (list-set local-member)
        (map (& local-bindings plural-frame) b (& content b)))
      (const member-translations (list-set member-translation)
        (map plural-members m (new member-translation m (instantiate-member m (cons singular-frame env)))))
      (function (translate-member (m local-member)) local-member
        (const mi member-translation (unique-elt-of member-translations mi (= (& plural-member mi) m local-member)))
        (return (& singular-member mi)))
      (&= local-bindings singular-frame (map (& local-bindings plural-frame) b (set-field b content (translate-member (& content b)))))
      (&= parameters singular-frame (map (&opt parameters plural-frame) op
                                         (new parameter (assert-in (translate-member (& var op)) (union dynamic-var variable)) (& default op))))
      (if (in (&opt rest plural-frame) (tag none))
        (&= rest singular-frame none)
        (&= rest singular-frame (assert-in (translate-member (assert-in (&opt rest plural-frame) variable)) variable)))
      (return singular-frame))
    
    
    
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
        (const g (union package global-object) (get-package-or-global-frame env))
        (when (in g global-object)
          (note "Try to write the variable into " (:local g) " again, this time allowing new dynamic bindings to be created dynamically.")
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
    
    
    (define (find-local-member (o (union frame simple-instance reg-exp date)) (multiname multiname) (access access))
            local-member-opt
      (const matching-local-bindings (list-set local-binding)
        (map (& local-bindings o) b b (and (set-in (& qname b) multiname) (accesses-overlap (& accesses b) access))))
      (note "If the same member was found via several different bindings " (:local b)
            ", then it will appear only once in the set " (:local matching-local-members) ".")
      (const matching-local-members (list-set local-member) (map matching-local-bindings b (& content b)))
      (cond
       ((empty matching-local-members)
        (return none))
       ((= (length matching-local-members) 1)
        (return (unique-elt-of matching-local-members)))
       (nil
        (note "This access is ambiguous because the bindings it found belong to several different local members.")
        (throw property-access-error))))
    
    
    (define (instance-member-accesses (m instance-member)) access-set
      (case m
        (:select (union instance-variable instance-method) (return read-write))
        (:select instance-getter (return read))
        (:select instance-setter (return write))))
    
    
    (define (find-local-instance-member (c class) (multiname multiname) (accesses access-set))
            instance-member-opt
      (const matching-members (list-set instance-member)
        (map (& instance-members c) m m (and (nonempty (set* (&opt multiname m) multiname)) (accesses-overlap (instance-member-accesses m) accesses))))
      (cond
       ((empty matching-members)
        (return none))
       ((= (length matching-members) 1)
        (return (unique-elt-of matching-members)))
       (nil
        (note "This access is ambiguous because it found several different instance members in the same class.")
        (throw property-access-error))))
    
    
    (define (find-common-member (o object) (multiname multiname) (access access) (flat boolean))
            (union (tag none) local-member instance-member)
      (var m (union (tag none) local-member instance-member))
      (case o
        (:select (union undefined null boolean long u-long float32 float64 character string namespace compound-attribute method-closure)
          (return none))
        (:narrow package
          (return (find-local-member o multiname access)))
        (:narrow (union simple-instance reg-exp date global-object)
          (<- m (find-local-member o multiname access)))
        (:narrow class
          (<- m (find-local-member o multiname access))
          (when (in m (tag none))
            (<- m (find-local-instance-member o multiname access)))))
      (rwhen (not-in m (tag none))
        (return m))
      (const parent object-opt (& parent (assert-in o (union simple-instance reg-exp date global-object class))))
      (when (not-in parent (tag none) :narrow-true)
        (<- m (find-common-member parent multiname access flat))
        (when (and flat (in m dynamic-var))
          (<- m none)))
      (return m))
    
    
    (define (find-base-instance-member (c class) (multiname multiname) (accesses access-set))
            instance-member-opt
      (note "Start from the root class (" (:character-literal "Object") ") and proceed through more specific classes that are ancestors of " (:local c) ".")
      (for-each (ancestors c) s
        (const m instance-member-opt (find-local-instance-member s multiname accesses))
        (rwhen (not-in m (tag none))
          (return m)))
      (return none))
    
    
    (%text :comment (:global-call get-derived-instance-member c m-base accesses) " returns the most derived instance member whose name includes that of " (:local m-base)
           " and whose access includes " (:local access) ". The caller of " (:global get-derived-instance-member) " ensures that such a member always exists. "
           "If " (:local accesses) " is " (:tag read-write) " then it is possible that this search could find both a getter and a setter defined in the same class; "
           "in this case either the getter or the setter is returned at the implementation" :apostrophe "s discretion.")
    (define (get-derived-instance-member (c class) (m-base instance-member) (accesses access-set)) instance-member
      (reserve m)
      (if (some (& instance-members c) m (and (set<= (&opt multiname m-base) (&opt multiname m) multiname) (accesses-overlap (instance-member-accesses m) accesses)) :define-true)
        (return m)
        (return (get-derived-instance-member (assert-not-in (& super c) (tag none)) m-base accesses))))
    
    
    ;***** Used for initialisation only
    (define (lookup-instance-member (c class) (qname qualified-name) (access access)) instance-member-opt
      (const m-base instance-member-opt (find-base-instance-member c (list-set qname) access))
      (rwhen (in m-base (tag none) :narrow-false)
        (return none))
      (return (get-derived-instance-member c m-base access)))
    
    
    
    (%heading (3 :semantics) "Reading a Property")
    (define (read-property (container (union obj-optional-limit frame)) (multiname multiname) (kind lookup-kind) (phase phase))
            object-opt
      (case container
        (:narrow object
          (const c class (object-type container))
          (const m-base instance-member-opt (find-base-instance-member c multiname read))
          (rwhen (not-in m-base (tag none) :narrow-true)
            (return (read-instance-member container c m-base phase)))
          (const m (union (tag none) local-member instance-member) (find-common-member container multiname read false))
          (case m
            (:select (tag none)
              (if (and (in kind (tag property-lookup))
                       (in container (union simple-instance date reg-exp global-object) :narrow-true)
                       (not (& sealed container)))
                (case phase
                  (:select (tag compile) (throw compile-expression-error))
                  (:select (tag run) (return undefined)))
                (return none)))
            (:narrow local-member (return (read-local-member m phase)))
            (:narrow instance-member
              (rwhen (or (not-in container class :narrow-false) (in kind (tag property-lookup) :narrow-false))
                (throw property-access-error))
              (const this object-i-opt (& this kind))
              (case this
                (:select (tag none) (throw property-access-error))
                (:select (tag inaccessible) (throw compile-expression-error))
                (:narrow object (return (read-instance-member this (object-type this) m phase)))))))
        (:narrow (union system-frame parameter-frame block-frame)
          (const m local-member-opt (find-local-member container multiname read))
          (if (in m (tag none) :narrow-false)
            (return none)
            (return (read-local-member m phase))))
        (:narrow limited-instance
          (const superclass class-opt (& super (& limit container)))
          (rwhen (in superclass (tag none) :narrow-false)
            (return none))
          (const m-base instance-member-opt (find-base-instance-member superclass multiname read))
          (if (not-in m-base (tag none) :narrow-true)
            (return (read-instance-member (& instance container) superclass m-base phase))
            (return none)))))
    
    
    (define (read-instance-member (this object) (c class) (m-base instance-member) (phase phase))
            object
      (const m instance-member (get-derived-instance-member c m-base read))
      (case m
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
    
    
    (define (read-local-member (m local-member) (phase phase)) object
      (case m
        (:select (tag forbidden) (throw property-access-error))
        (:narrow variable
          (rwhen (and (in phase (tag compile)) (not (& immutable m)))
            (throw compile-expression-error))
          (const value variable-value (& value m))
          (case value
            (:narrow object (return value))
            (:select (tag inaccessible)
              (if (in phase (tag compile))
                (throw compile-expression-error)
                (throw uninitialised-error)))
            (:select (tag uninitialised)
              (throw uninitialised-error))
            (:select uninstantiated-function
              (assert (in phase (tag compile))
                "An uninstantiated function can only be found when " (:assertion) ".")
              (throw compile-expression-error))
            (:narrow (-> () object)
              (assert (in phase (tag compile))
                (:assertion) " because all futures are resolved by the end of the compilation phase.")
              (&= value m inaccessible)
              (const type class (get-variable-type m phase))
              (const new-value object (value))
              (const coerced-value object ((&opt implicit-coerce type) new-value false))
              (&= value m coerced-value)
              (return new-value))))
        (:narrow dynamic-var
          (rwhen (in phase (tag compile))
            (throw compile-expression-error))
          (const value (union object uninstantiated-function) (& value m))
          (assert (not-in value uninstantiated-function :narrow-true)
            (:local value) " can be an " (:type uninstantiated-function) " only during the " (:tag compile) " phase, which was ruled out above.")
          (return value))
        (:narrow constructor-method (return (& code m)))
        (:narrow getter
          (const env environment-i (& env m))
          (rwhen (in env (tag inaccessible) :narrow-false)
            (throw compile-expression-error))
          (return ((& call m) env phase)))
        (:narrow setter
          (bottom (:local m) " cannot be a " (:type setter) " because these are only represented as write-only members."))))
    
    
    (%heading (3 :semantics) "Writing a Property")
    (define (write-property (container (union obj-optional-limit frame)) (multiname multiname) (kind lookup-kind) (create-if-missing boolean)
                            (new-value object) (phase (tag run)))
            (tag none ok)
      (case container
        (:narrow object
          (const c class (object-type container))
          (const m-base instance-member-opt (find-base-instance-member c multiname write))
          (rwhen (not-in m-base (tag none) :narrow-true)
            (write-instance-member container c m-base new-value phase)
            (return ok))
          (const m (union (tag none) local-member instance-member) (find-common-member container multiname write true))
          (case m
            (:select (tag none)
              (when (and create-if-missing
                         (in container (union simple-instance date reg-exp global-object) :narrow-true)
                         (not (& sealed container)))
                (const qname qualified-name (select-primary-name multiname))
                (note "Before trying to create a new dynamic property named " (:local qname)
                      ", check that there is no read-only fixed property with the same name.")
                (rwhen (and (in (find-base-instance-member c (list-set qname) read) (tag none))
                            (in (find-common-member container (list-set qname) read true) (tag none)))
                  (const dv dynamic-var (new dynamic-var new-value false))
                  (&= local-bindings container (set+ (& local-bindings container) (list-set (new local-binding qname read-write dv false))))
                  (return ok)))
              (return none))
            (:narrow local-member
              (write-local-member m new-value phase)
              (return ok))
            (:narrow instance-member
              (rwhen (or (not-in container class :narrow-false) (in kind (tag property-lookup) :narrow-false))
                (throw property-access-error))
              (note (:local this) " cannot be " (:tag inaccessible) " during the " (:tag run) " phase.")
              (const this object-opt (assert-not-in (& this kind) (tag inaccessible)))
              (case this
                (:select (tag none) (throw property-access-error))
                (:narrow object
                  (write-instance-member this (object-type this) m new-value phase)
                  (return ok))))))
        (:narrow (union system-frame parameter-frame block-frame)
          (const m local-member-opt (find-local-member container multiname write))
          (cond
           ((in m (tag none) :narrow-false)
            (return none))
           (nil
            (write-local-member m new-value phase)
            (return ok))))
        (:narrow limited-instance
          (const superclass class-opt (& super (& limit container)))
          (rwhen (in superclass (tag none) :narrow-false)
            (return none))
          (const m-base instance-member-opt (find-base-instance-member superclass multiname write))
          (cond
           ((not-in m-base (tag none) :narrow-true)
            (write-instance-member (& instance container) superclass m-base new-value phase)
            (return ok))
           (nil (return none))))))
    
    
    (define (select-primary-name (multiname multiname)) qualified-name
      (reserve qname)
      (cond
       ((= (length multiname) 1)
        (return (unique-elt-of multiname)))
       ((some multiname qname (= (& namespace qname) public-namespace namespace) :define-true)
        (return qname))
       (nil (throw property-access-error))))
    
    
    (define (write-instance-member (this object) (c class) (m-base instance-member) (new-value object) (phase (tag run)))
            void
      (const m instance-member (get-derived-instance-member c m-base write))
      (case m
        (:narrow instance-variable
          (const s slot (find-slot this m))
          (rwhen (and (& immutable m) (not-in (& value s) (tag uninitialised)))
            (throw property-access-error))
          (const coerced-value object ((&opt implicit-coerce (&opt type m)) new-value false))
          (&= value s coerced-value))
        (:select instance-method
          (throw property-access-error))
        (:narrow instance-getter
          (bottom (:local m) " cannot be an " (:type instance-getter) " because these are only represented as read-only members."))
        (:narrow instance-setter
          (const coerced-value object ((&opt implicit-coerce (&opt type m)) new-value false))
          ((& call m) this coerced-value (& env m) phase))))
    
    
    (define (write-local-member (m local-member) (new-value object) (phase (tag run))) void
      (case m
        (:select (union (tag forbidden) constructor-method) (throw property-access-error))
        (:narrow variable
          (rwhen (or (in (& value m) (tag inaccessible))
                     (and (& immutable m) (not-in (& value m) (tag uninitialised))))
            (throw property-access-error))
          (const type class (get-variable-type m phase))
          (const coerced-value object ((&opt implicit-coerce type) new-value false))
          (&= value m coerced-value))
        (:narrow dynamic-var
          (&= value m new-value))
        (:narrow getter
          (bottom (:local m) " cannot be a " (:type getter) " because these are only represented as read-only members."))
        (:narrow setter
          (const coerced-value object ((&opt implicit-coerce (& type m)) new-value false))
          (const env environment-i (& env m))
          (assert (not-in env (tag inaccessible) :narrow-true)
            "All instances are resolved for the " (:tag run) " phase, so " (:assertion) ".")
          ((& call m) coerced-value env phase))))
    
    
    (define (get-variable-type (v variable) (phase phase)) class
      (const type variable-type (& type v))
      (case type
        (:narrow class (return type))
        (:select (tag inaccessible)
          (assert (in phase (tag compile))
            "This can only happen when " (:assertion) " because the compilation phase ensures that all types are valid, "
            "so invalid types will not occur during the run phase.")
          (throw compile-expression-error))
        (:narrow (-> () class)
          (assert (in phase (tag compile))
            (:assertion) " because all futures are resolved by the end of the compilation phase.")
          (&= type v inaccessible)
          (const new-type class (type))
          (&= type v new-type)
          (return new-type))))
    
    
    (%heading (3 :semantics) "Deleting a Property")
    (define (delete-property (container (union obj-optional-limit frame)) (multiname multiname) (kind lookup-kind) (phase (tag run) :unused))
            boolean-opt
      (case container
        (:narrow object
          (const c class (object-type container))
          (rwhen (not-in (find-base-instance-member c multiname write) (tag none))
            (return false))
          (const m (union (tag none) local-member instance-member) (find-common-member container multiname write true))
          (case m
            (:select (tag none) (return none))
            (:select (tag forbidden) (throw property-access-error))
            (:select (union variable constructor-method getter setter) (return false))
            (:narrow dynamic-var
              (cond
               ((& sealed m) (return false))
               (nil
                (&= local-bindings (assert-in container (union class simple-instance reg-exp date package global-object))
                    (map (& local-bindings (assert-in container (union class simple-instance reg-exp date package global-object)))
                         b b (or (set-not-in (& qname b) multiname) (/= (& content b) m local-member))))
                (return true))))
            (:narrow instance-member
              (rwhen (or (not-in container class :narrow-false) (in kind (tag property-lookup) :narrow-false))
                (return false))
              (note (:local this) " cannot be " (:tag inaccessible) " during the " (:tag run) " phase.")
              (const this object-opt (assert-not-in (& this kind) (tag inaccessible)))
              (case this
                (:select (tag none) (throw property-access-error))
                (:narrow object (return false))))))
        (:narrow (union system-frame parameter-frame block-frame)
          (if (not-in (find-local-member container multiname write) (tag none))
            (return false)
            (return none)))
        (:narrow limited-instance
          (const superclass class-opt (& super (& limit container)))
          (rwhen (in superclass (tag none) :narrow-false)
            (return none))
          (if (not-in (find-base-instance-member superclass multiname write) (tag none))
            (return false)
            (return none)))))
    
    
    
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
      (production :identifier (include) identifier-include (name "include")))
    (%print-actions)
    
    (%heading 2 "Qualified Identifiers")
    (rule :qualifier ((open-namespaces (writable-cell (list-set namespace)))
                      (validate (-> (context environment) void))
                      (eval (-> (environment phase) namespace)))
      (production :qualifier (:identifier) qualifier-identifier
        ((validate cxt (env :unused))
         (action<- (open-namespaces :qualifier 0) (& open-namespaces cxt)))
        ((eval env phase)
         (const multiname multiname (map (open-namespaces :qualifier 0) ns (new qualified-name ns (name :identifier))))
         (const a object (lexical-read env multiname phase))
         (rwhen (not-in a namespace :narrow-false) (throw bad-value-error))
         (return a)))
      (production :qualifier (public) qualifier-public
        ((validate (cxt :unused) (env :unused)))
        ((eval (env :unused) (phase :unused)) (return public-namespace)))
      (production :qualifier (private) qualifier-private
        ((validate (cxt :unused) env)
         (const c class-opt (get-enclosing-class env))
         (rwhen (in c (tag none))
           (throw syntax-error)))
        ((eval env (phase :unused))
         (const c class-opt (get-enclosing-class env))
         (assert (not-in c (tag none) :narrow-true)
           (:action validate) " already ensured that " (:assertion) ".")
         (return (& private-namespace c)))))
    
    (rule :simple-qualified-identifier ((open-namespaces (writable-cell (list-set namespace)))
                                        (validate (-> (context environment) void))
                                        (setup (-> () void))
                                        (eval (-> (environment phase) multiname)))
      (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier
        ((validate cxt (env :unused))
         (action<- (open-namespaces :simple-qualified-identifier 0) (& open-namespaces cxt)))
        ((setup))
        ((eval (env :unused) (phase :unused))
         (return (map (open-namespaces :simple-qualified-identifier 0) ns (new qualified-name ns (name :identifier))))))
      (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier
        ((validate cxt env)
         ((validate :qualifier) cxt env))
        ((setup))
        ((eval env phase)
         (const q namespace ((eval :qualifier) env phase))
         (return (list-set (new qualified-name q (name :identifier)))))))
    
    (rule :expression-qualified-identifier ((validate (-> (context environment) void))
                                            (setup (-> () void))
                                            (eval (-> (environment phase) multiname)))
      (production :expression-qualified-identifier (:paren-expression \:\: :identifier) expression-qualified-identifier-identifier
        ((validate cxt env)
         ((validate :paren-expression) cxt env))
        ((setup) ((setup :paren-expression)))
        ((eval env phase)
         (const q object (read-reference ((eval :paren-expression) env phase) phase))
         (rwhen (not-in q namespace :narrow-false) (throw bad-value-error))
         (return (list-set (new qualified-name q (name :identifier)))))))
    
    (rule :qualified-identifier ((validate (-> (context environment) void))
                                 (setup (-> () void))
                                 (eval (-> (environment phase) multiname)))
      (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :simple-qualified-identifier) env phase))))
      (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :expression-qualified-identifier) env phase)))))
    (%print-actions ("Validation" open-namespaces validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Primary Expressions")
    (rule :primary-expression ((validate (-> (context environment) void)) (setup (-> () void)) (eval (-> (environment phase) obj-or-ref)))
      (production :primary-expression (null) primary-expression-null
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return null)))
      (production :primary-expression (true) primary-expression-true
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return true)))
      (production :primary-expression (false) primary-expression-false
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return false)))
      (production :primary-expression (public) primary-expression-public
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return public-namespace)))
      (production :primary-expression ($number) primary-expression-number
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (value $number))))
      (production :primary-expression ($string) primary-expression-string
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (value $string))))
      (production :primary-expression (this) primary-expression-this
        ((validate (cxt :unused) env)
         (rwhen (in (find-this env true) (tag none))
           (throw syntax-error)))
        ((setup) :forward)
        ((eval env (phase :unused))
         (const this object-i-opt (find-this env true))
         (assert (not-in this (tag none) :narrow-true)
           (:action validate) " ensured that " (:assertion) " at this point.")
         (rwhen (in this (tag inaccessible) :narrow-false)
           (throw compile-expression-error))
         (return this)))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (todo)))
      (production :primary-expression (:paren-list-expression) primary-expression-paren-list-expression
        ((validate cxt env) ((validate :paren-list-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :paren-list-expression) env phase))))
      (production :primary-expression (:array-literal) primary-expression-array-literal
        ((validate cxt env) ((validate :array-literal) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :array-literal) env phase))))
      (production :primary-expression (:object-literal) primary-expression-object-literal
        ((validate cxt env) ((validate :object-literal) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :object-literal) env phase))))
      (production :primary-expression (:function-expression) primary-expression-function-expression
        ((validate cxt env) ((validate :function-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :function-expression) env phase)))))
    
    (rule :paren-expression ((validate (-> (context environment) void)) (setup (-> () void)) (eval (-> (environment phase) obj-or-ref)))
      (production :paren-expression (\( (:assignment-expression allow-in) \)) paren-expression-assignment-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :assignment-expression) env phase)))))
    
    (rule :paren-list-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                  (eval (-> (environment phase) obj-or-ref))
                                  (eval-as-list (-> (environment phase) (vector object))))
      (production :paren-list-expression (:paren-expression) paren-list-expression-paren-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :paren-expression) env phase)))
        ((eval-as-list env phase)
         (const elt object (read-reference ((eval :paren-expression) env phase) phase))
         (return (vector elt))))
      (production :paren-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) paren-list-expression-list-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (exec (read-reference ((eval :list-expression) env phase) phase))
         (return (read-reference ((eval :assignment-expression) env phase) phase)))
        ((eval-as-list env phase)
         (const elts (vector object) ((eval-as-list :list-expression) env phase))
         (const elt object (read-reference ((eval :assignment-expression) env phase) phase))
         (return (append elts (vector elt))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Function Expressions")
    (rule :function-expression ((f (writable-cell uninstantiated-function))
                                (validate (-> (context environment) void))
                                (setup (-> () void))
                                (eval (-> (environment phase) obj-or-ref)))
      (production :function-expression (function :function-common) function-expression-anonymous
        ((validate cxt env)
         (const unchecked boolean (and (not (& strict cxt)) (plain :function-common)))
         (const this (tag none inaccessible) (if unchecked inaccessible none))
         (action<- (f :function-expression 0) ((validate-static-function :function-common) cxt env this unchecked unchecked)))
        ((setup) ((setup :function-common)))
        ((eval env phase)
         (rwhen (in phase (tag compile))
           (throw compile-expression-error))
         (return (instantiate-function (f :function-expression 0) env))))
      (production :function-expression (function :identifier :function-common) function-expression-named
        ((validate cxt env)
         (const v variable (new variable class-class inaccessible true))
         (const b local-binding (new local-binding (new qualified-name public-namespace (name :identifier)) read-write v false))
         (const compile-frame block-frame (new block-frame (list-set b) plural))
         (const unchecked boolean (and (not (& strict cxt)) (plain :function-common)))
         (const this (tag none inaccessible) (if unchecked inaccessible none))
         (action<- (f :function-expression 0) ((validate-static-function :function-common) cxt (cons compile-frame env) this unchecked unchecked)))
        ((setup) ((setup :function-common)))
        ((eval env phase)
         (rwhen (in phase (tag compile))
           (throw compile-expression-error))
         (const v variable (new variable class-class inaccessible true))
         (const b local-binding (new local-binding (new qualified-name public-namespace (name :identifier)) read-write v false))
         (const runtime-frame block-frame (new block-frame (list-set b) plural))
         (const f2 simple-instance (instantiate-function (f :function-expression 0) (cons runtime-frame env)))
         (&= value v f2)
         (return f2))))
    (%print-actions ("Validation" f validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Object Literals")
    (rule :object-literal ((validate (-> (context environment) void)) (setup (-> () void))
                           (eval (-> (environment phase) obj-or-ref)))
      (production :object-literal (\{ :field-list \}) object-literal-list
        ((validate cxt env) ((validate :field-list) cxt env))
        ((setup) ((setup :field-list)))
        ((eval env phase)
         (rwhen (in phase (tag compile))
           (throw compile-expression-error))
         (const local-bindings (list-set local-binding) ((eval :field-list) env phase))
         (return (new simple-instance local-bindings object-prototype false prototype-class (list-set-of slot) none none none none)))))
    
    
    (rule :field-list ((validate (-> (context environment) void)) (setup (-> () void))
                       (eval (-> (environment phase) (list-set local-binding))))
      (production :field-list () field-list-empty
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (list-set-of local-binding))))
      (production :field-list (:nonempty-field-list) field-list-nonempty
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :nonempty-field-list) env phase)))))
    
    
    (rule :nonempty-field-list ((validate (-> (context environment) void)) (setup (-> () void))
                                (eval (-> (environment phase) (list-set local-binding))))
      (production :nonempty-field-list (:literal-field) nonempty-field-list-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const b local-binding ((eval :literal-field) env phase))
         (return (list-set b))))
      (production :nonempty-field-list (:nonempty-field-list \, :literal-field) nonempty-field-list-more
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const local-bindings (list-set local-binding) ((eval :nonempty-field-list) env phase))
         (const b local-binding ((eval :literal-field) env phase))
         (rwhen (some local-bindings b2 (= (& qname b2) (& qname b) qualified-name))
           (throw argument-mismatch-error))
         (return (set+ local-bindings (list-set b))))))
    
    
    (rule :literal-field ((validate (-> (context environment) void))
                          (setup (-> () void))
                          (eval (-> (environment phase) local-binding)))
      (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression
        ((validate cxt env)
         ((validate :field-name) cxt env)
         ((validate :assignment-expression) cxt env))
        ((setup)
         ((setup :field-name))
         ((setup :assignment-expression)))
        ((eval env phase)
         (const qname qualified-name ((eval :field-name) env phase))
         (const value object (read-reference ((eval :assignment-expression) env phase) phase))
         (const p dynamic-var (new dynamic-var value false))
         (return (new local-binding qname read-write p false)))))
    
    
    (rule :field-name ((validate (-> (context environment) void)) (setup (-> () void))
                       (eval (-> (environment phase) qualified-name)))
      (production :field-name (:qualified-identifier) field-name-identifier
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return (select-primary-name ((eval :qualified-identifier) env phase)))))
      (production :field-name ($string) field-name-string
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) phase) (return (to-qualified-name (value $string) phase))))
      (production :field-name ($number) field-name-number
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) phase) (return (to-qualified-name (value $number) phase))))
      (production :field-name (:paren-expression) field-name-paren-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :paren-expression) env phase) phase))
         (return (to-qualified-name a phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Array Literals")
    (rule :array-literal ((validate (-> (context environment) void)) (setup (-> () void))
                          (eval (-> (environment phase) obj-or-ref)))
      (production :array-literal ([ :element-list ]) array-literal-list
        ((validate (cxt :unused) (env :unused)) (todo))
        ((setup) (todo))
        ((eval (env :unused) (phase :unused)) (todo))))
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Super Expressions")
    (rule :super-expression ((validate (-> (context environment) void)) (setup (-> () void))
                             (eval (-> (environment phase) obj-optional-limit)))
      (production :super-expression (super) super-expression-super
        ((validate (cxt :unused) env)
         (rwhen (or (in (get-enclosing-class env) (tag none)) (in (find-this env false) (tag none)))
           (throw syntax-error)))
        ((setup) :forward)
        ((eval env phase)
         (const this object-i-opt (find-this env false))
         (assert (not-in this (tag none) :narrow-true)
           (:action validate) " ensured that " (:local this) " cannot be " (:tag none) " at this point.")
         (rwhen (in this (tag inaccessible) :narrow-false)
           (throw compile-expression-error))
         (const limit class-opt (get-enclosing-class env))
         (assert (not-in limit (tag none) :narrow-true)
           (:action validate) " ensured that " (:local limit) " cannot be " (:tag none) " at this point.")
         (return (read-limited-reference this limit phase))))
      (production :super-expression (super :paren-expression) super-expression-super-paren-expression
        ((validate cxt env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw syntax-error))
         ((validate :paren-expression) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :paren-expression) env phase))
         (const limit class-opt (get-enclosing-class env))
         (assert (not-in limit (tag none) :narrow-true)
           (:action validate) " ensured that " (:local limit) " cannot be " (:tag none) " at this point.")
         (return (read-limited-reference r limit phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%text :comment (:global-call read-limited-reference r phase) " reads the reference, if any, inside " (:local r)
           " and returns the result, retaining " (:local limit)
           ". The object read from the reference is checked to make sure that it is an instance of " (:local limit)
           " or one of its descendants. If "
           (:local phase) " is " (:tag compile) ", only compile-time expressions can be evaluated in the process of reading " (:local r) ".")
    (define (read-limited-reference (r obj-or-ref) (limit class) (phase phase)) obj-optional-limit
      (const o object (read-reference r phase))
      (rwhen (in o (tag null))
        (return null))
      (rwhen (not ((&opt is-instance-of limit) o))
        (throw bad-value-error))
      (return (new limited-instance o limit)))
    
    
    (%heading 2 "Postfix Expressions")
    (rule :postfix-expression ((validate (-> (context environment) void)) (setup (-> () void))
                               (eval (-> (environment phase) obj-or-ref)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :attribute-expression) env phase))))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :full-postfix-expression) env phase))))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :short-new-expression) env phase)))))
    
    (rule :attribute-expression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (setup (-> () void))
                                 (eval (-> (environment phase) obj-or-ref)))
      (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier
        ((validate cxt env)
         ((validate :simple-qualified-identifier) cxt env)
         (action<- (strict :attribute-expression 0) (& strict cxt)))
        ((setup) :forward)
        ((eval env phase)
         (const m multiname ((eval :simple-qualified-identifier) env phase))
         (return (new lexical-reference env m (strict :attribute-expression 0)))))
      (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator
        ((validate cxt env)
         ((validate :attribute-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :attribute-expression) env phase) phase))
         (return ((eval :member-operator) env a phase))))
      (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call
        ((validate cxt env)
         ((validate :attribute-expression) cxt env)
         ((validate :arguments) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :attribute-expression) env phase))
         (const f object (read-reference r phase))
         (const base object (reference-base r))
         (const args (vector object) ((eval :arguments) env phase))
         (return (call base f args phase)))))
    
    (rule :full-postfix-expression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (setup (-> () void))
                                    (eval (-> (environment phase) obj-or-ref)))
      (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression
        ((validate cxt env) ((validate :primary-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :primary-expression) env phase))))
      (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier
        ((validate cxt env)
         ((validate :expression-qualified-identifier) cxt env)
         (action<- (strict :full-postfix-expression 0) (& strict cxt)))
        ((setup) :forward)
        ((eval env phase)
         (const m multiname ((eval :expression-qualified-identifier) env phase))
         (return (new lexical-reference env m (strict :full-postfix-expression 0)))))
      (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression
        ((validate cxt env) ((validate :full-new-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :full-new-expression) env phase))))
      (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator
        ((validate cxt env)
         ((validate :full-postfix-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :full-postfix-expression) env phase) phase))
         (return ((eval :member-operator) env a phase))))
      (production :full-postfix-expression (:super-expression :member-operator) full-postfix-expression-super-member-operator
        ((validate cxt env)
         ((validate :super-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a obj-optional-limit ((eval :super-expression) env phase))
         (return ((eval :member-operator) env a phase))))
      (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call
        ((validate cxt env)
         ((validate :full-postfix-expression) cxt env)
         ((validate :arguments) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :full-postfix-expression) env phase))
         (const f object (read-reference r phase))
         (const base object (reference-base r))
         (const args (vector object) ((eval :arguments) env phase))
         (return (call base f args phase))))
      (production :full-postfix-expression (:postfix-expression :no-line-break ++) full-postfix-expression-increment
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((setup) :forward)
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
        ((setup) :forward)
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (const a object (read-reference r phase))
         (const b object (plus a phase))
         (const c object (subtract b 1.0 phase))
         (write-reference r c phase)
         (return b))))
    
    (rule :full-new-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                (eval (-> (environment phase) obj-or-ref)))
      (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const f object (read-reference ((eval :full-new-subexpression) env phase) phase))
         (const args (vector object) ((eval :arguments) env phase))
         (return (construct f args phase)))))
    
    (rule :full-new-subexpression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (setup (-> () void))
                                   (eval (-> (environment phase) obj-or-ref)))
      (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression
        ((validate cxt env) ((validate :primary-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :primary-expression) env phase))))
      (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier
        ((validate cxt env)
         ((validate :qualified-identifier) cxt env)
         (action<- (strict :full-new-subexpression 0) (& strict cxt)))
        ((setup) :forward)
        ((eval env phase)
         (const m multiname ((eval :qualified-identifier) env phase))
         (return (new lexical-reference env m (strict :full-new-subexpression 0)))))
      (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression
        ((validate cxt env) ((validate :full-new-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :full-new-expression) env phase))))
      (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator
        ((validate cxt env)
         ((validate :full-new-subexpression) cxt env)
         ((validate :member-operator) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :full-new-subexpression) env phase) phase))
         (return ((eval :member-operator) env a phase))))
      (production :full-new-subexpression (:super-expression :member-operator) full-new-subexpression-super-member-operator
        ((validate cxt env)
         ((validate :super-expression) cxt env)
         ((validate :member-operator) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a obj-optional-limit ((eval :super-expression) env phase))
         (return ((eval :member-operator) env a phase)))))
    
    (rule :short-new-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                 (eval (-> (environment phase) obj-or-ref)))
      (production :short-new-expression (new :short-new-subexpression) short-new-expression-new
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const f object (read-reference ((eval :short-new-subexpression) env phase) phase))
         (return (construct f (vector-of object) phase)))))
    
    (rule :short-new-subexpression ((validate (-> (context environment) void)) (setup (-> () void))
                                    (eval (-> (environment phase) obj-or-ref)))
      (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :full-new-subexpression) env phase))))
      (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :short-new-expression) env phase)))))
    (%print-actions ("Validation" strict validate) ("Setup" setup) ("Evaluation" eval))
    
    
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
    
    
    (define (call (this object) (a object) (args (vector object)) (phase phase)) object
      (case a
        (:select (union undefined null boolean general-number character string namespace compound-attribute date reg-exp package global-object)
          (throw bad-value-error))
        (:narrow class
          (return ((& call a) this args phase)))
        (:narrow simple-instance
          (const f (union (-> (object (vector object) environment phase) object) (tag none)) (& call a))
          (rwhen (in f (tag none) :narrow-false)
            (throw bad-value-error))
          (return (f this args (assert-not-in (& env a) (tag none)) phase)))
        (:narrow method-closure
          (return ((& call (& method a)) (& this a) args phase)))))
    
    (define (construct (a object) (args (vector object)) (phase phase)) object
      (case a
        (:select (union undefined null boolean general-number character string namespace compound-attribute method-closure date reg-exp package global-object)
          (throw bad-value-error))
        (:narrow class
          (return ((& construct a) args phase)))
        (:narrow simple-instance
          (const f (union (-> ((vector object) environment phase) object) (tag none)) (& construct a))
          (rwhen (in f (tag none) :narrow-false)
            (throw bad-value-error))
          (return (f args (assert-not-in (& env a) (tag none)) phase)))))
    
    
    (%heading 2 "Member Operators")
    (rule :member-operator ((validate (-> (context environment) void)) (setup (-> () void))
                            (eval (-> (environment obj-optional-limit phase) obj-or-ref)))
      (production :member-operator (\. :qualified-identifier) member-operator-qualified-identifier
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env base phase)
         (const m multiname ((eval :qualified-identifier) env phase))
         (return (new dot-reference base m))))
      (production :member-operator (:brackets) member-operator-brackets
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env base phase)
         (const args (vector object) ((eval :brackets) env phase))
         (return (new bracket-reference base args)))))
    
    (rule :brackets ((validate (-> (context environment) void)) (setup (-> () void))
                     (eval (-> (environment phase) (vector object))))
      (production :brackets ([ ]) brackets-none
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (vector-of object))))
      (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed
        ((validate cxt env) ((validate :list-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval-as-list :list-expression) env phase)))))
    
    (rule :arguments ((validate (-> (context environment) void)) (setup (-> () void))
                      (eval (-> (environment phase) (vector object))))
      (production :arguments (\( \)) arguments-none
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (vector-of object))))
      (production :arguments (:paren-list-expression) arguments-some
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval-as-list :paren-list-expression) env phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Unary Operators")
    (rule :unary-expression ((strict (writable-cell boolean)) (validate (-> (context environment) void)) (setup (-> () void))
                             (eval (-> (environment phase) obj-or-ref)))
      (production :unary-expression (:postfix-expression) unary-expression-postfix
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :postfix-expression) env phase))))
      (production :unary-expression (delete :postfix-expression) unary-expression-delete
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         (action<- (strict :unary-expression 0) (& strict cxt)))
        ((setup) :forward)
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (return (delete-reference r (strict :unary-expression 0) phase))))
      (production :unary-expression (void :unary-expression) unary-expression-void
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (exec (read-reference ((eval :unary-expression) env phase) phase))
         (return undefined)))
      (production :unary-expression (typeof :unary-expression) unary-expression-typeof
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :unary-expression) env phase) phase))
         (const c class (object-type a))
         (return (& typeof-string c))))
      (production :unary-expression (++ :postfix-expression) unary-expression-increment
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((setup) :forward)
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
        ((setup) :forward)
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
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :unary-expression) env phase) phase))
         (return (plus a phase))))
      (production :unary-expression (- :unary-expression) unary-expression-minus
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :unary-expression) env phase) phase))
         (return (minus a phase))))
      (production :unary-expression (- $negated-min-long) unary-expression-min-long
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused))
         (return (new long (neg (expt 2 63))))))
      (production :unary-expression (~ :unary-expression) unary-expression-bitwise-not
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :unary-expression) env phase) phase))
         (return (bit-not a phase))))
      (production :unary-expression (! :unary-expression) unary-expression-logical-not
        ((validate cxt env) ((validate :unary-expression) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :unary-expression) env phase) phase))
         (return (logical-not a phase)))))
    (%print-actions ("Validation" strict validate) ("Setup" setup) ("Evaluation" eval))
    
    
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
    (rule :multiplicative-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                      (eval (-> (environment phase) obj-or-ref)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :unary-expression) env phase))))
      (production :multiplicative-expression (:multiplicative-expression * :unary-expression) multiplicative-expression-multiply
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :multiplicative-expression) env phase) phase))
         (const b object (read-reference ((eval :unary-expression) env phase) phase))
         (return (multiply a b phase))))
      (production :multiplicative-expression (:multiplicative-expression / :unary-expression) multiplicative-expression-divide
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :multiplicative-expression) env phase) phase))
         (const b object (read-reference ((eval :unary-expression) env phase) phase))
         (return (divide a b phase))))
      (production :multiplicative-expression (:multiplicative-expression % :unary-expression) multiplicative-expression-remainder
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :multiplicative-expression) env phase) phase))
         (const b object (read-reference ((eval :unary-expression) env phase) phase))
         (return (remainder a b phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
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
    (rule :additive-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                (eval (-> (environment phase) obj-or-ref)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :multiplicative-expression) env phase))))
      (production :additive-expression (:additive-expression + :multiplicative-expression) additive-expression-add
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :additive-expression) env phase) phase))
         (const b object (read-reference ((eval :multiplicative-expression) env phase) phase))
         (return (add a b phase))))
      (production :additive-expression (:additive-expression - :multiplicative-expression) additive-expression-subtract
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :additive-expression) env phase) phase))
         (const b object (read-reference ((eval :multiplicative-expression) env phase) phase))
         (return (subtract a b phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
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
    (rule :shift-expression ((validate (-> (context environment) void)) (setup (-> () void))
                             (eval (-> (environment phase) obj-or-ref)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :additive-expression) env phase))))
      (production :shift-expression (:shift-expression << :additive-expression) shift-expression-left
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :shift-expression) env phase) phase))
         (const b object (read-reference ((eval :additive-expression) env phase) phase))
         (return (shift-left a b phase))))
      (production :shift-expression (:shift-expression >> :additive-expression) shift-expression-right-signed
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :shift-expression) env phase) phase))
         (const b object (read-reference ((eval :additive-expression) env phase) phase))
         (return (shift-right a b phase))))
      (production :shift-expression (:shift-expression >>> :additive-expression) shift-expression-right-unsigned
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :shift-expression) env phase) phase))
         (const b object (read-reference ((eval :additive-expression) env phase) phase))
         (return (shift-right-unsigned a b phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
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
    (rule (:relational-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                          (eval (-> (environment phase) obj-or-ref)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
       ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :shift-expression) env phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) < :shift-expression) relational-expression-less
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (return (is-less a b phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) > :shift-expression) relational-expression-greater
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (return (is-less b a phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) <= :shift-expression) relational-expression-less-or-equal
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (return (is-less-or-equal a b phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) >= :shift-expression) relational-expression-greater-or-equal
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (return (is-less-or-equal b a phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) is :shift-expression) relational-expression-is
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (todo)))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (const c class (to-class b))
         (return ((&opt implicit-coerce c) a true))))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression) relational-expression-in
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (const name string (to-string a phase))
         (const qname qualified-name (new qualified-name public-namespace name))
         (const c class (object-type b))
         (return (or (not-in (find-base-instance-member c (list-set qname) read) (tag none))
                     (not-in (find-base-instance-member c (list-set qname) write) (tag none))
                     (not-in (find-common-member b (list-set qname) read false) (tag none))
                     (not-in (find-common-member b (list-set qname) write false) (tag none))))))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
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
    (rule (:equality-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                        (eval (-> (environment phase) obj-or-ref)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :relational-expression) env phase))))
      (production (:equality-expression :beta) ((:equality-expression :beta) == (:relational-expression :beta)) equality-expression-equal
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :equality-expression) env phase) phase))
         (const b object (read-reference ((eval :relational-expression) env phase) phase))
         (return (is-equal a b phase))))
      (production (:equality-expression :beta) ((:equality-expression :beta) != (:relational-expression :beta)) equality-expression-not-equal
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :equality-expression) env phase) phase))
         (const b object (read-reference ((eval :relational-expression) env phase) phase))
         (return (not (is-equal a b phase)))))
      (production (:equality-expression :beta) ((:equality-expression :beta) === (:relational-expression :beta)) equality-expression-strict-equal
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :equality-expression) env phase) phase))
         (const b object (read-reference ((eval :relational-expression) env phase) phase))
         (return (is-strictly-equal a b phase))))
      (production (:equality-expression :beta) ((:equality-expression :beta) !== (:relational-expression :beta)) equality-expression-strict-not-equal
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :equality-expression) env phase) phase))
         (const b object (read-reference ((eval :relational-expression) env phase) phase))
         (return (not (is-strictly-equal a b phase))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
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
        (:select (union namespace compound-attribute class method-closure simple-instance date reg-exp package global-object)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace compound-attribute class method-closure simple-instance date reg-exp package global-object) (return (is-strictly-equal a b phase)))
            (:select (union boolean general-number character string)
              (const ap primitive-object (to-primitive a null phase))
              (return (is-equal ap b phase)))))))
    
    (define (is-strictly-equal (a object) (b object) (phase phase :unused)) boolean
      (cond
       ((and (in a general-number :narrow-true) (in b general-number :narrow-true))
        (return (= (general-number-compare a b) equal order)))
       (nil
        (return (= a b object)))))
    
    
    (%heading 2 "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                           (eval (-> (environment phase) obj-or-ref)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :equality-expression) env phase))))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression :beta) & (:equality-expression :beta)) bitwise-and-expression-and
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :bitwise-and-expression) env phase) phase))
         (const b object (read-reference ((eval :equality-expression) env phase) phase))
         (return (bit-and a b phase)))))
    
    (rule (:bitwise-xor-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                           (eval (-> (environment phase) obj-or-ref)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :bitwise-and-expression) env phase))))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression :beta) ^ (:bitwise-and-expression :beta)) bitwise-xor-expression-xor
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :bitwise-xor-expression) env phase) phase))
         (const b object (read-reference ((eval :bitwise-and-expression) env phase) phase))
         (return (bit-xor a b phase)))))
    
    (rule (:bitwise-or-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                          (eval (-> (environment phase) obj-or-ref)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :bitwise-xor-expression) env phase))))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression :beta) \| (:bitwise-xor-expression :beta)) bitwise-or-expression-or
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :bitwise-or-expression) env phase) phase))
         (const b object (read-reference ((eval :bitwise-xor-expression) env phase) phase))
         (return (bit-or a b phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
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
    (rule (:logical-and-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                           (eval (-> (environment phase) obj-or-ref)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :bitwise-or-expression) env phase))))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :logical-and-expression) env phase) phase))
         (if (to-boolean a phase)
           (return (read-reference ((eval :bitwise-or-expression) env phase) phase))
           (return a)))))
    
    (rule (:logical-xor-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                           (eval (-> (environment phase) obj-or-ref)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :logical-and-expression) env phase))))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :logical-xor-expression) env phase) phase))
         (const b object (read-reference ((eval :logical-and-expression) env phase) phase))
         (const ba boolean (to-boolean a phase))
         (const bb boolean (to-boolean b phase))
         (return (xor ba bb)))))
    
    (rule (:logical-or-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                          (eval (-> (environment phase) obj-or-ref)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :logical-xor-expression) env phase))))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :logical-or-expression) env phase) phase))
         (if (to-boolean a phase)
           (return a)
           (return (read-reference ((eval :logical-xor-expression) env phase) phase))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Conditional Operator")
    (rule (:conditional-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                           (eval (-> (environment phase) obj-or-ref)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :logical-or-expression) env phase))))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :logical-or-expression) env phase) phase))
         (if (to-boolean a phase)
           (return (read-reference ((eval :assignment-expression 1) env phase) phase))
           (return (read-reference ((eval :assignment-expression 2) env phase) phase))))))
    
    (rule (:non-assignment-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                              (eval (-> (environment phase) obj-or-ref)))
      (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :logical-or-expression) env phase))))
      (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :logical-or-expression) env phase) phase))
         (if (to-boolean a phase)
           (return (read-reference ((eval :non-assignment-expression 1) env phase) phase))
           (return (read-reference ((eval :non-assignment-expression 2) env phase) phase))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Assignment Operators")
    (rule (:assignment-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                          (eval (-> (environment phase) obj-or-ref)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        ((validate cxt env) ((validate :conditional-expression) cxt env))
        ((setup) ((setup :conditional-expression)))
        ((eval env phase) (return ((eval :conditional-expression) env phase))))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((setup)
         ((setup :postfix-expression))
         ((setup :assignment-expression)))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const ra obj-or-ref ((eval :postfix-expression) env phase))
         (const b object (read-reference ((eval :assignment-expression) env phase) phase))
         (write-reference ra b phase)
         (return b)))
      (production (:assignment-expression :beta) (:postfix-expression :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((setup)
         ((setup :postfix-expression))
         ((setup :assignment-expression)))
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw compile-expression-error))
         (const r-left obj-or-ref ((eval :postfix-expression) env phase))
         (const o-left object (read-reference r-left phase))
         (const o-right object (read-reference ((eval :assignment-expression) env phase) phase))
         (const result object ((op :compound-assignment) o-left o-right phase))
         (write-reference r-left result phase)
         (return result)))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((validate cxt env)
         ((validate :postfix-expression) cxt env)
         ((validate :assignment-expression) cxt env))
        ((setup)
         ((setup :postfix-expression))
         ((setup :assignment-expression)))
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
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" op operator eval))
    
    
    (%heading 2 "Comma Expressions")
    (rule (:list-expression :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                    (eval (-> (environment phase) obj-or-ref))
                                    (eval-as-list (-> (environment phase) (vector object))))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :assignment-expression) env phase)))
        ((eval-as-list env phase)
         (const elt object (read-reference ((eval :assignment-expression) env phase) phase))
         (return (vector elt))))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (exec (read-reference ((eval :list-expression) env phase) phase))
         (return (read-reference ((eval :assignment-expression) env phase) phase)))
        ((eval-as-list env phase)
         (const elts (vector object) ((eval-as-list :list-expression) env phase))
         (const elt object (read-reference ((eval :assignment-expression) env phase) phase))
         (return (append elts (vector elt))))))
    
    (production :optional-expression ((:list-expression allow-in)) optional-expression-expression)
    (production :optional-expression () optional-expression-empty)
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Type Expressions")
    (rule (:type-expression :beta) ((validate (-> (context environment) void))
                                    (setup-and-eval (-> (environment) class)))
      (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression
        ((validate cxt env) ((validate :non-assignment-expression) cxt env))
        ((setup-and-eval env)
         ((setup :non-assignment-expression))
         (const o object (read-reference ((eval :non-assignment-expression) env compile) compile))
         (return (to-class o)))))
    (%print-actions ("Validation" validate) ("Setup and Evaluation" setup-and-eval))
    
    
    (%heading 1 "Statements")
    
    (grammar-argument :omega
                      abbrev       ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)        ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (rule (:statement :omega) ((validate (-> (context environment (list-set label) jump-targets plurality) void)) (setup (-> () void))
                               (eval (-> (environment object) object)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :expression-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :expression-statement) env))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :super-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :super-statement) env))))
      (production (:statement :omega) (:block) statement-block
        ((validate cxt env (sl :unused) jt pl) ((validate :block) cxt env jt pl))
        ((setup) :forward)
        ((eval env d) (return ((eval :block) env d))))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        ((validate cxt env sl jt (pl :unused)) ((validate :labeled-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :labeled-statement) env d))))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        ((validate cxt env (sl :unused) jt (pl :unused)) ((validate :if-statement) cxt env jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :if-statement) env d))))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((validate cxt env (sl :unused) jt (pl :unused)) ((validate :switch-statement) cxt env jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :switch-statement) env d))))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((validate cxt env sl jt (pl :unused)) ((validate :do-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :do-statement) env d))))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((validate cxt env sl jt (pl :unused)) ((validate :while-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :while-statement) env d))))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((validate cxt env sl jt (pl :unused)) ((validate :for-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :for-statement) env d))))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((validate cxt env (sl :unused) jt (pl :unused)) ((validate :with-statement) cxt env jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :with-statement) env d))))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) jt (pl :unused)) ((validate :continue-statement) jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :continue-statement) env d))))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) jt (pl :unused)) ((validate :break-statement) jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :break-statement) env d))))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :return-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :return-statement) env))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((validate cxt env (sl :unused) (jt :unused) (pl :unused)) ((validate :throw-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :throw-statement) env))))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((validate cxt env (sl :unused) jt (pl :unused)) ((validate :try-statement) cxt env jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :try-statement) env d)))))
    
    
    (rule (:substatement :omega) ((enabled (writable-cell boolean))
                                  (validate (-> (context environment (list-set label) jump-targets) void))
                                  (setup (-> () void))
                                  (eval (-> (environment object) object)))
      (production (:substatement :omega) (:empty-statement) substatement-empty-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused)))
        ((setup))
        ((eval (env :unused) d) (return d)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        ((validate cxt env sl jt) ((validate :statement) cxt env sl jt plural))
        ((setup) ((setup :statement)))
        ((eval env d) (return ((eval :statement) env d))))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((validate cxt env (sl :unused) (jt :unused)) ((validate :simple-variable-definition) cxt env))
        ((setup) ((setup :simple-variable-definition)))
        ((eval env d) (return ((eval :simple-variable-definition) env d))))
      (production (:substatement :omega) (:attributes :no-line-break { :substatements }) substatement-annotated-group
        ((validate cxt env (sl :unused) jt)
         ((validate :attributes) cxt env)
         ((setup :attributes))
         (const attr attribute ((eval :attributes) env compile))
         (rwhen (not-in attr boolean :narrow-false)
           (throw bad-value-error))
         (action<- (enabled :substatement 0) attr)
         (when attr
           ((validate :substatements) cxt env jt)))
        ((setup)
         (when (enabled :substatement 0)
           ((setup :substatements))))
        ((eval env d)
         (if (enabled :substatement 0)
           (return ((eval :substatements) env d))
           (return d)))))
    
    
    (rule :substatements ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                          (eval (-> (environment object) object)))
      (production :substatements () substatements-none
        ((validate (cxt :unused) (env :unused) (jt :unused)))
        ((setup) :forward)
        ((eval (env :unused) d) (return d)))
      (production :substatements (:substatements-prefix (:substatement abbrev)) substatements-more
        ((validate cxt env jt)
         ((validate :substatements-prefix) cxt env jt)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((setup) :forward)
        ((eval env d)
         (const o object ((eval :substatements-prefix) env d))
         (return ((eval :substatement) env o)))))
    
    (rule :substatements-prefix ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                                 (eval (-> (environment object) object)))
      (production :substatements-prefix () substatements-prefix-none
        ((validate (cxt :unused) (env :unused) (jt :unused)))
        ((setup) :forward)
        ((eval (env :unused) d) (return d)))
      (production :substatements-prefix (:substatements-prefix (:substatement full)) substatements-prefix-more
        ((validate cxt env jt)
         ((validate :substatements-prefix) cxt env jt)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((setup) :forward)
        ((eval env d)
         (const o object ((eval :substatements-prefix) env d))
         (return ((eval :substatement) env o)))))
    
    
    (rule (:semicolon :omega) ((setup (-> () void)))
      (production (:semicolon :omega) (\;) semicolon-semicolon
        ((setup)))
      (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon
        ((setup)))
      (production (:semicolon abbrev) () semicolon-abbrev
        ((setup)))
      (production (:semicolon no-short-if) () semicolon-no-short-if
        ((setup))))
    (%print-actions ("Validation" enabled validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%heading 2 "Expression Statement")
    (rule :expression-statement ((validate (-> (context environment) void)) (setup (-> () void))
                                 (eval (-> (environment) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        ((validate cxt env) ((validate :list-expression) cxt env))
        ((setup) ((setup :list-expression)))
        ((eval env)
         (return (read-reference ((eval :list-expression) env run) run)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Super Statement")
    (rule :super-statement ((validate (-> (context environment) void)) (setup (-> () void))
                            (eval (-> (environment) object)))
      (production :super-statement (super :arguments) super-statement-super-arguments
        ((validate (cxt :unused) (env :unused)) (todo))
        ((setup) ((setup :arguments)))
        ((eval (env :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Block Statement")
    (rule :block ((compile-frame (writable-cell block-frame))
                  (validate (-> (context environment jump-targets plurality) void))
                  (validate-using-frame (-> (context environment jump-targets plurality frame) void))
                  (setup (-> () void))
                  (eval (-> (environment object) object))
                  (eval-using-frame (-> (environment frame object) object)))
      (production :block ({ :directives }) block-directives
        ((validate cxt env jt pl)
         (const compile-frame block-frame (new block-frame (list-set-of local-binding) pl))
         (action<- (compile-frame :block 0) compile-frame)
         (exec ((validate :directives) cxt (cons compile-frame env) jt pl none)))
        ((validate-using-frame cxt env jt pl frame)
         (exec ((validate :directives) cxt (cons frame env) jt pl none)))
        ((setup) ((setup :directives)))
        ((eval env d)
         (const compile-frame block-frame (compile-frame :block 0))
         (var runtime-frame block-frame)
         (case (& plurality compile-frame)
           (:select (tag singular) (<- runtime-frame compile-frame))
           (:select (tag plural) (<- runtime-frame (instantiate-block-frame compile-frame env))))
         (return ((eval :directives) (cons runtime-frame env) d)))
        ((eval-using-frame env frame d)
         (return ((eval :directives) (cons frame env) d)))))
    (%print-actions ("Validation" compile-frame validate validate-using-frame) ("Setup" setup) ("Evaluation" eval eval-using-frame))
    
    
    (%heading 2 "Labeled Statements")
    (rule (:labeled-statement :omega) ((validate (-> (context environment (list-set label) jump-targets) void)) (setup (-> () void))
                                       (eval (-> (environment object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((validate cxt env sl jt)
         (const name string (name :identifier))
         (rwhen (set-in name (& break-targets jt))
           (throw syntax-error))
         (const jt2 jump-targets (new jump-targets (set+ (& break-targets jt) (list-set-of label name)) (& continue-targets jt)))
         ((validate :substatement) cxt env (set+ sl (list-set-of label name)) jt2))
        ((setup) ((setup :substatement)))
        ((eval env d)
         (catch ((return ((eval :substatement) env d)))
           (x) (if (and (in x break :narrow-true) (= (& label x) (name :identifier) label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "If Statement")
    (rule (:if-statement :omega) ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                                  (eval (-> (environment object) object)))
      (production (:if-statement abbrev) (if :paren-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((setup) :forward)
        ((eval env d)
         (const o object (read-reference ((eval :paren-list-expression) env run) run))
         (if (to-boolean o run)
           (return ((eval :substatement) env d))
           (return d))))
      (production (:if-statement full) (if :paren-list-expression (:substatement full)) if-statement-if-then-full
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((setup) :forward)
        ((eval env d)
         (const o object (read-reference ((eval :paren-list-expression) env run) run))
         (if (to-boolean o run)
           (return ((eval :substatement) env d))
           (return d))))
      (production (:if-statement :omega) (if :paren-list-expression (:substatement no-short-if) else (:substatement :omega))
                  if-statement-if-then-else
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement 1) cxt env (list-set-of label) jt)
         ((validate :substatement 2) cxt env (list-set-of label) jt))
        ((setup) :forward)
        ((eval env d)
         (const o object (read-reference ((eval :paren-list-expression) env run) run))
         (if (to-boolean o run)
           (return ((eval :substatement 1) env d))
           (return ((eval :substatement 2) env d))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Switch Statement")
    (deftuple switch-key
      (key object))
    (deftype switch-guard (union switch-key (tag default) object))
    
    (rule :switch-statement ((compile-frame (writable-cell block-frame))
                             (validate (-> (context environment jump-targets) void)) (setup (-> () void))
                             (eval (-> (environment object) object)))
      (production :switch-statement (switch :paren-list-expression { :case-elements }) switch-statement-cases
        ((validate cxt env jt)
         (rwhen (> (n-defaults :case-elements) 1)
           (throw syntax-error))
         ((validate :paren-list-expression) cxt env)
         (const jt2 jump-targets (new jump-targets
                                   (set+ (& break-targets jt) (list-set-of label default))
                                   (& continue-targets jt)))
         (const compile-frame block-frame (new block-frame (list-set-of local-binding) plural))
         (action<- (compile-frame :switch-statement 0) compile-frame)
         (exec ((validate :case-elements) cxt (cons compile-frame env) jt2)))
        ((setup) :forward)
        ((eval env d)
         (const key object (read-reference ((eval :paren-list-expression) env run) run))
         (const compile-frame block-frame (compile-frame :switch-statement 0))
         (const runtime-frame block-frame (instantiate-block-frame compile-frame env))
         (const runtime-env environment (cons runtime-frame env))
         (var result switch-guard ((eval :case-elements) runtime-env (new switch-key key) d))
         (rwhen (in result object :narrow-true)
           (return result))
         (assert (= result (new switch-key key) switch-guard))
         (<- result ((eval :case-elements) runtime-env default d))
         (rwhen (in result object :narrow-true)
           (return result))
         (assert (= result default switch-guard))
         (return d))))
    
    (rule :case-elements ((n-defaults integer)
                          (validate (-> (context environment jump-targets) context)) (setup (-> () void))
                          (eval (-> (environment switch-guard object) switch-guard)))
      (production :case-elements () case-elements-none
        (n-defaults 0)
        ((validate cxt (env :unused) (jt :unused))
         (return cxt))
        ((setup) :forward)
        ((eval (env :unused) guard (d :unused)) (return guard)))
      (production :case-elements (:case-label) case-elements-one
        (n-defaults (n-defaults :case-label))
        ((validate cxt env (jt :unused))
         ((validate :case-label) cxt env)
         (return cxt))
        ((setup) :forward)
        ((eval env guard d)
         (return ((eval :case-label) env guard d))))
      (production :case-elements (:case-label :case-elements-prefix (:case-element abbrev)) case-elements-more
        (n-defaults (+ (n-defaults :case-label) (+ (n-defaults :case-elements-prefix) (n-defaults :case-element))))
        ((validate cxt env jt)
         ((validate :case-label) cxt env)
         (const cxt2 context ((validate :case-elements-prefix) cxt env jt))
         (return ((validate :case-element) cxt2 env jt)))
        ((setup) :forward)
        ((eval env guard d)
         (const guard2 switch-guard ((eval :case-label) env guard d))
         (const guard3 switch-guard ((eval :case-elements-prefix) env guard2 d))
         (return ((eval :case-element) env guard3 d)))))
    
    (rule :case-elements-prefix ((n-defaults integer)
                                 (validate (-> (context environment jump-targets) context)) (setup (-> () void))
                                 (eval (-> (environment switch-guard object) switch-guard)))
      (production :case-elements-prefix () case-elements-prefix-none
        (n-defaults 0)
        ((validate cxt (env :unused) (jt :unused))
         (return cxt))
        ((setup) :forward)
        ((eval (env :unused) guard (d :unused)) (return guard)))
      (production :case-elements-prefix (:case-elements-prefix (:case-element full)) case-elements-prefix-more
        (n-defaults (+ (n-defaults :case-elements-prefix) (n-defaults :case-element)))
        ((validate cxt env jt)
         (const cxt2 context ((validate :case-elements-prefix) cxt env jt))
         (return ((validate :case-element) cxt2 env jt)))
        ((setup) :forward)
        ((eval env guard d)
         (const guard2 switch-guard ((eval :case-elements-prefix) env guard d))
         (return ((eval :case-element) env guard2 d)))))
    
    (rule (:case-element :omega_2) ((n-defaults integer)
                                    (validate (-> (context environment jump-targets) context)) (setup (-> () void))
                                    (eval (-> (environment switch-guard object) switch-guard)))
      (production (:case-element :omega_2) ((:directive :omega_2)) case-element-directive
        (n-defaults 0)
        ((validate cxt env jt)
         (return ((validate :directive) cxt env jt plural none)))
        ((setup) :forward)
        ((eval env guard (d :unused))
         (case guard
           (:narrow (union switch-key (tag default)) (return guard))
           (:narrow object
             (return ((eval :directive) env guard))))))
      (production (:case-element :omega_2) (:case-label) case-element-case-label
        (n-defaults (n-defaults :case-label))
        ((validate cxt env (jt :unused))
         ((validate :case-label) cxt env)
         (return cxt))
        ((setup) :forward)
        ((eval env guard d)
         (return ((eval :case-label) env guard d)))))
    
    (rule :case-label ((n-defaults integer)
                       (validate (-> (context environment) void)) (setup (-> () void))
                       (eval (-> (environment switch-guard object) switch-guard)))
      (production :case-label (case (:list-expression allow-in) \:) case-label-case
        (n-defaults 0)
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env guard d)
         (case guard
           (:narrow (union (tag default) object) (return guard))
           (:narrow switch-key
             (const label object (read-reference ((eval :list-expression) env run) run))
             (if (is-strictly-equal (& key guard) label run)
               (return d)
               (return guard))))))
      (production :case-label (default \:) case-label-default
        (n-defaults 1)
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) guard d)
         (case guard
           (:narrow (union switch-key object) (return guard))
           (:narrow (tag default) (return d))))))
    (%print-actions ("Validation" n-defaults compile-frame validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Do-While Statement")
    (rule :do-statement ((labels (writable-cell (list-set label)))
                         (validate (-> (context environment (list-set label) jump-targets) void)) (setup (-> () void))
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
        ((setup) :forward)
        ((eval env d)
         (catch ((var d1 object d)
                 (while true
                   (catch ((<- d1 ((eval :substatement) env d1)))
                     (x) (if (and (in x continue :narrow-true) (set-in (& label x) (labels :do-statement 0)))
                           (<- d1 (& value x))
                           (throw x)))
                   (const o object (read-reference ((eval :paren-list-expression) env run) run))
                   (rwhen (not (to-boolean o run))
                     (return d1))))
           (x) (if (and (in x break :narrow-true) (= (& label x) default label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" labels validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "While Statement")
    (rule (:while-statement :omega) ((labels (writable-cell (list-set label)))
                                     (validate (-> (context environment (list-set label) jump-targets) void)) (setup (-> () void))
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
        ((setup) :forward)
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
    (%print-actions ("Validation" labels validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "For Statements")
    (rule (:for-statement :omega) ((validate (-> (context environment (list-set label) jump-targets) void)) (setup (-> () void))
                                   (eval (-> (environment object) object)))
      (production (:for-statement :omega) (for \( :for-initialiser \; :optional-expression \; :optional-expression \)
                                               (:substatement :omega)) for-statement-c-style
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused)) (todo))
        ((setup) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:substatement :omega)) for-statement-in
        ((validate (cxt :unused) (env :unused) (sl :unused) (jt :unused)) (todo))
        ((setup) (todo))
        ((eval (env :unused) (d :unused)) (todo))))
    
    (production :for-initialiser () for-initialiser-empty)
    (production :for-initialiser ((:list-expression no-in)) for-initialiser-expression)
    (production :for-initialiser (:variable-definition-kind (:variable-binding-list no-in)) for-initialiser-variable-definition)
    (production :for-initialiser (:attributes :no-line-break :variable-definition-kind (:variable-binding-list no-in)) for-initialiser-attribute-variable-definition)
    
    (production :for-in-binding (:postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    (production :for-in-binding (:attributes :no-line-break :variable-definition-kind (:variable-binding no-in)) for-in-binding-attribute-variable-definition)
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "With Statement")
    (rule (:with-statement :omega) ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                                    (eval (-> (environment object) object)))
      (production (:with-statement :omega) (with :paren-list-expression (:substatement :omega)) with-statement-with
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((setup) :forward)
        ((eval (env :unused) (d :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Continue and Break Statements")
    (rule :continue-statement ((validate (-> (jump-targets) void)) (setup (-> () void))
                               (eval (-> (environment object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((validate jt)
         (rwhen (set-not-in default (& continue-targets jt))
           (throw syntax-error)))
        ((setup))
        ((eval (env :unused) d) (throw (new continue d default))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((validate jt)
         (rwhen (set-not-in (name :identifier) (& continue-targets jt))
           (throw syntax-error)))
        ((setup))
        ((eval (env :unused) d) (throw (new continue d (name :identifier))))))
    
    (rule :break-statement ((validate (-> (jump-targets) void)) (setup (-> () void))
                            (eval (-> (environment object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((validate jt)
         (rwhen (set-not-in default (& break-targets jt))
           (throw syntax-error)))
        ((setup))
        ((eval (env :unused) d) (throw (new break d default))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((validate jt)
         (rwhen (set-not-in (name :identifier) (& break-targets jt))
           (throw syntax-error)))
        ((setup))
        ((eval (env :unused) d) (throw (new break d (name :identifier))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Return Statement")
    (rule :return-statement ((validate (-> (context environment) void)) (setup (-> () void))
                             (eval (-> (environment) object)))
      (production :return-statement (return) return-statement-default
        ((validate (cxt :unused) env)
         (rwhen (not-in (get-regional-frame env) parameter-frame)
           (throw syntax-error)))
        ((setup) :forward)
        ((eval (env :unused)) (throw (new returned-value undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((validate cxt env)
         (rwhen (not-in (get-regional-frame env) parameter-frame)
           (throw syntax-error))
         ((validate :list-expression) cxt env))
        ((setup) :forward)
        ((eval env)
         (const a object (read-reference ((eval :list-expression) env run) run))
         (throw (new returned-value a)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Throw Statement")
    (rule :throw-statement ((validate (-> (context environment) void)) (setup (-> () void))
                            (eval (-> (environment) object)))
      (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw
        ((validate cxt env) ((validate :list-expression) cxt env))
        ((setup) ((setup :list-expression)))
        ((eval env)
         (const a object (read-reference ((eval :list-expression) env run) run))
         (throw (new thrown-value a)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Try Statement")
    (rule :try-statement ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                          (eval (-> (environment object) object)))
      (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses
        ((validate cxt env jt)
         ((validate :block) cxt env jt plural)
         ((validate :catch-clauses) cxt env jt))
        ((setup) :forward)
        ((eval env d)
         (catch ((return ((eval :block) env d)))
           (x)
           (rwhen (not-in x thrown-value :narrow-false)
             (throw x))
           (const exception object (& value x))
           (const r (union object (tag reject)) ((eval :catch-clauses) env exception))
           (if (not-in r (tag reject) :narrow-true)
             (return r)
             (throw x)))))
      
      (production :try-statement (try :block :catch-clauses-opt finally :block) try-statement-catch-clauses-finally
        ((validate cxt env jt)
         ((validate :block 1) cxt env jt plural)
         ((validate :catch-clauses-opt) cxt env jt)
         ((validate :block 2) cxt env jt plural))
        ((setup) :forward)
        ((eval env d)
         (var result (union object semantic-exception))
         (catch ((<- result ((eval :block 1) env d)))
           (x) (<- result x))
         (when (in result thrown-value)
           (const exception object (& value (assert-in result thrown-value)))
           (catch ((const r (union object (tag reject)) ((eval :catch-clauses-opt) env exception))
                   (when (not-in r (tag reject) :narrow-true)
                     (<- result r)))
             (y) (<- result y)))
         (exec ((eval :block 2) env undefined))
         (case result
           (:narrow object (return result))
           (:narrow semantic-exception (throw result))))))
    
    (rule :catch-clauses-opt ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                              (eval (-> (environment object) (union object (tag reject)))))
      (production :catch-clauses-opt () catch-clauses-opt-none
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval (env :unused) (exception :unused))
         (return reject)))
      (production :catch-clauses-opt (:catch-clauses) catch-clauses-opt-some
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval env exception)
         (return ((eval :catch-clauses) env exception)))))
    
    (rule :catch-clauses ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                          (eval (-> (environment object) (union object (tag reject)))))
      (production :catch-clauses (:catch-clause) catch-clauses-one
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval env exception)
         (return ((eval :catch-clause) env exception))))
      (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval env exception)
         (const r (union object (tag reject)) ((eval :catch-clauses) env exception))
         (if (not-in r (tag reject) :narrow-true)
           (return r)
           (return ((eval :catch-clause) env exception))))))
    
    (rule :catch-clause ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                         (eval (-> (environment object) (union object (tag reject)))))
      (production :catch-clause (catch \( :parameter \) :block) catch-clause-block
        ((validate (cxt :unused) (env :unused) (jt :unused)) (todo))
        ((setup) (todo))
        ((eval (env :unused) (exception :unused)) (todo))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 1 "Directives")
    (rule (:directive :omega_2) ((enabled (writable-cell boolean))
                                 (validate (-> (context environment jump-targets plurality attribute-opt-not-false) context))
                                 (setup (-> () void))
                                 (eval (-> (environment object) object)))
      (production (:directive :omega_2) (:empty-statement) directive-empty-statement
        ((validate cxt (env :unused) (jt :unused) (pl :unused) (attr :unused)) (return cxt))
        ((setup))
        ((eval (env :unused) d) (return d)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        ((validate cxt env jt pl attr)
         (rwhen (not-in attr (tag none true))
           (throw syntax-error))
         ((validate :statement) cxt env (list-set-of label) jt pl)
         (return cxt))
        ((setup) ((setup :statement)))
        ((eval env d) (return ((eval :statement) env d))))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((validate cxt env (jt :unused) pl attr) (return ((validate :annotatable-directive) cxt env pl attr)))
        ((setup) ((setup :annotatable-directive)))
        ((eval env d) (return ((eval :annotatable-directive) env d))))
      (production (:directive :omega_2) (:attributes :no-line-break (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((validate cxt env (jt :unused) pl attr)
         ((validate :attributes) cxt env)
         ((setup :attributes))
         (const attr2 attribute ((eval :attributes) env compile))
         (const attr3 attribute (combine-attributes attr attr2))
         (action<- (enabled :directive 0) (not-in attr3 false-type))
         (if (not-in attr3 false-type :narrow-true)
           (return ((validate :annotatable-directive) cxt env pl attr3))
           (return cxt)))
        ((setup)
         (when (enabled :directive 0)
           ((setup :annotatable-directive))))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :annotatable-directive) env d))
           (return d))))
      (production (:directive :omega_2) (:attributes :no-line-break { :directives }) directive-annotated-group
        ((validate cxt env jt pl attr)
         ((validate :attributes) cxt env)
         ((setup :attributes))
         (const attr2 attribute ((eval :attributes) env compile))
         (const attr3 attribute (combine-attributes attr attr2))
         (action<- (enabled :directive 0) (not-in attr3 false-type))
         (rwhen (in attr3 false-type :narrow-false)
           (return cxt))
         (return ((validate :directives) cxt env jt pl attr3)))
        ((setup)
         (when (enabled :directive 0)
           ((setup :directives))))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :directives) env d))
           (return d))))
      (production (:directive :omega_2) (:package-definition) directive-package-definition
        ((validate (cxt :unused) (env :unused) (jt :unused) (pl :unused) attr) (if (in attr (tag none true)) (todo) (throw syntax-error)))
        ((setup) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((validate (cxt :unused) (env :unused) (jt :unused) (pl :unused) attr) (if (in attr (tag none true)) (todo) (throw syntax-error)))
          ((setup) (todo))
          ((eval (env :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((validate cxt (env :unused) (jt :unused) (pl :unused) attr) (if (in attr (tag none true)) (return ((validate :pragma) cxt)) (throw syntax-error)))
        ((setup))
        ((eval (env :unused) d) (return d))))
    
    (rule (:annotatable-directive :omega_2) ((validate (-> (context environment plurality attribute-opt-not-false) context))
                                             (setup (-> () void))
                                             (eval (-> (environment object) object)))
      (production (:annotatable-directive :omega_2) (:export-definition (:semicolon :omega_2)) annotatable-directive-export-definition
        ((validate (cxt :unused) (env :unused) (pl :unused) (attr :unused)) (todo))
        ((setup) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:variable-definition (:semicolon :omega_2)) annotatable-directive-variable-definition
        ((validate cxt env (pl :unused) attr)
         ((validate :variable-definition) cxt env attr)
         (return cxt))
        ((setup) ((setup :variable-definition)))
        ((eval env d) (return ((eval :variable-definition) env d))))
      (production (:annotatable-directive :omega_2) (:function-definition) annotatable-directive-function-definition
        ((validate cxt env pl attr)
         ((validate :function-definition) cxt env pl attr)
         (return cxt))
        ((setup) ((setup :function-definition)))
        ((eval (env :unused) d) (return d)))
      (production (:annotatable-directive :omega_2) (:class-definition) annotatable-directive-class-definition
        ((validate cxt env pl attr)
         ((validate :class-definition) cxt env pl attr)
         (return cxt))
        ((setup) ((setup :class-definition)))
        ((eval env d) (return ((eval :class-definition) env d))))
      (production (:annotatable-directive :omega_2) (:namespace-definition (:semicolon :omega_2)) annotatable-directive-namespace-definition
        ((validate cxt env pl attr)
         ((validate :namespace-definition) cxt env pl attr)
         (return cxt))
        ((setup))
        ((eval (env :unused) d) (return d)))
      ;(production (:annotatable-directive :omega_2) ((:interface-definition :omega_2)) annotatable-directive-interface-definition
      ;  ((validate (cxt :unused) (env :unused) (pl :unused) (attr :unused)) (todo))
      ;  ((setup) (todo))
      ;  ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:import-directive (:semicolon :omega_2)) annotatable-directive-import-directive
        ((validate (cxt :unused) (env :unused) (pl :unused) (attr :unused)) (todo))
        ((setup) (todo))
        ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:use-directive (:semicolon :omega_2)) annotatable-directive-use-directive
        ((validate cxt env (pl :unused) attr)
         (if (in attr (tag none true))
           (return ((validate :use-directive) cxt env))
           (throw syntax-error)))
        ((setup))
        ((eval (env :unused) d) (return d))))
    
    
    (rule :directives ((validate (-> (context environment jump-targets plurality attribute-opt-not-false) context)) (setup (-> () void))
                       (eval (-> (environment object) object)))
      (production :directives () directives-none
        ((validate cxt (env :unused) (jt :unused) (pl :unused) (attr :unused)) (return cxt))
        ((setup) :forward)
        ((eval (env :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((validate cxt env jt pl attr)
         (const cxt2 context ((validate :directives-prefix) cxt env jt pl attr))
         (return ((validate :directive) cxt2 env jt pl attr)))
        ((setup) :forward)
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    
    (rule :directives-prefix ((validate (-> (context environment jump-targets plurality attribute-opt-not-false) context)) (setup (-> () void))
                              (eval (-> (environment object) object)))
      (production :directives-prefix () directives-prefix-none
        ((validate cxt (env :unused) (jt :unused) (pl :unused) (attr :unused)) (return cxt))
        ((setup) :forward)
        ((eval (env :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((validate cxt env jt pl attr)
         (const cxt2 context ((validate :directives-prefix) cxt env jt pl attr))
         (return ((validate :directive) cxt2 env jt pl attr)))
        ((setup) :forward)
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Attributes")
    (rule :attributes ((validate (-> (context environment) void)) (setup (-> () void))
                       (eval (-> (environment phase) attribute)))
      (production :attributes (:attribute) attributes-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :attribute) env phase))))
      (production :attributes (:attribute-combination) attributes-attribute-combination
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :attribute-combination) env phase)))))
    
    
    (rule :attribute-combination ((validate (-> (context environment) void)) (setup (-> () void))
                                  (eval (-> (environment phase) attribute)))
      (production :attribute-combination (:attribute :no-line-break :attributes) attribute-combination-more
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a attribute ((eval :attribute) env phase))
         (rwhen (in a false-type :narrow-false)
           (return false))
         (const b attribute ((eval :attributes) env phase))
         (return (combine-attributes a b)))))
    
    
    (rule :attribute ((validate (-> (context environment) void)) (setup (-> () void))
                      (eval (-> (environment phase) attribute)))
      (production :attribute (:attribute-expression) attribute-attribute-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :attribute-expression) env phase) phase))
         (rwhen (not-in a attribute :narrow-false)
           (throw bad-value-error))
         (return a)))
      (production :attribute (true) attribute-true
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return true)))
      (production :attribute (false) attribute-false
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return false)))
      (production :attribute (public) attribute-public
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return public-namespace)))
      (production :attribute (:nonexpression-attribute) attribute-nonexpression-attribute
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :nonexpression-attribute) env phase)))))
    
    
    (rule :nonexpression-attribute ((validate (-> (context environment) void)) (setup (-> () void))
                                    (eval (-> (environment phase) attribute)))
      (production :nonexpression-attribute (final) nonexpression-attribute-final
        ((validate (cxt :unused) (env :unused)))
        ((setup))
        ((eval (env :unused) (phase :unused)) (return (new compound-attribute (list-set-of namespace) false false final none false false))))
      (production :nonexpression-attribute (private) nonexpression-attribute-private
        ((validate (cxt :unused) env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw syntax-error)))
        ((setup))
        ((eval env (phase :unused))
         (const c class-opt (get-enclosing-class env))
         (assert (not-in c (tag none) :narrow-true)
           (:action validate) " ensured that " (:local c) " cannot be " (:tag none) " at this point.")
         (return (& private-namespace c))))
      (production :nonexpression-attribute (static) nonexpression-attribute-static
        ((validate (cxt :unused) (env :unused)))
        ((setup))
        ((eval (env :unused) (phase :unused)) (return (new compound-attribute (list-set-of namespace) false false static none false false)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    
    (%heading 2 "Use Directive")
    (rule :use-directive ((validate (-> (context environment) context)))
      (production :use-directive (use namespace :paren-list-expression) use-directive-normal
        ((validate cxt env)
         ((validate :paren-list-expression) cxt env)
         ((setup :paren-list-expression))
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
    (rule :variable-definition ((validate (-> (context environment attribute-opt-not-false) void)) (setup (-> () void))
                                (eval (-> (environment object) object)))
      (production :variable-definition (:variable-definition-kind (:variable-binding-list allow-in)) variable-definition-definition
        ((validate cxt env attr)
         (const immutable boolean (immutable :variable-definition-kind))
         ((validate :variable-binding-list) cxt env attr immutable))
        ((setup) ((setup :variable-binding-list)))
        ((eval env d)
         (const immutable boolean (immutable :variable-definition-kind))
         ((eval :variable-binding-list) env immutable)
         (return d))))
    
    (rule :variable-definition-kind ((immutable boolean))
      (production :variable-definition-kind (var) variable-definition-kind-var (immutable false))
      (production :variable-definition-kind (const) variable-definition-kind-const (immutable true)))
    
    (rule (:variable-binding-list :beta) ((validate (-> (context environment attribute-opt-not-false boolean) void))
                                           (setup (-> () void))
                                          (eval (-> (environment boolean) void)))
      (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one
        ((validate cxt env attr immutable) :forward)
        ((setup) :forward)
        ((eval env immutable) ((eval :variable-binding) env immutable)))
      (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more
        ((validate cxt env attr immutable) :forward)
        ((setup) :forward)
        ((eval env immutable)
         ((eval :variable-binding-list) env immutable)
         ((eval :variable-binding) env immutable))))
    
    (rule (:variable-binding :beta) ((compile-env (writable-cell environment))
                                     (compile-var (writable-cell (union dynamic-var variable instance-variable)))
                                     (overridden-var (writable-cell instance-variable-opt))
                                     (multiname (writable-cell multiname))
                                     (validate (-> (context environment attribute-opt-not-false boolean) void))
                                     (setup (-> () void))
                                     (eval (-> (environment boolean) void)))
      (production (:variable-binding :beta) ((:typed-identifier :beta) (:variable-initialisation :beta)) variable-binding-full
        ((validate cxt env attr immutable)
         ((validate :typed-identifier) cxt env)
         ((validate :variable-initialisation) cxt env)
         (action<- (compile-env :variable-binding 0) env)
         (const name string (name :typed-identifier))
         (cond
          ((and (not (& strict cxt)) (in (get-regional-frame env) (union global-object parameter-frame))
                (not immutable) (in attr (tag none)) (plain :typed-identifier))
           (const qname qualified-name (new qualified-name public-namespace name))
           (action<- (multiname :variable-binding 0) (list-set qname))
           (action<- (compile-var :variable-binding 0) (define-hoisted-var env name undefined)))
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
           (case member-mod
             (:select (tag none static)
               (function (eval-type) class
                 (const type class-opt ((setup-and-eval :typed-identifier) env))
                 (rwhen (in type (tag none) :narrow-false)
                   (return object-class))
                 (return type))
               (function (eval-initialiser) object
                 ((setup :variable-initialisation))
                 (const value object-opt ((eval :variable-initialisation) env compile))
                 (rwhen (in value (tag none) :narrow-false)
                   (throw compile-expression-error))
                 (return value))
               (var initial-value variable-value inaccessible)
               (when immutable
                 (<- initial-value eval-initialiser))
               (const v variable (new variable eval-type initial-value immutable))
               (const multiname multiname (define-local-member env name (& namespaces a) (& override-mod a) (& explicit a) read-write v))
               (action<- (multiname :variable-binding 0) multiname)
               (action<- (compile-var :variable-binding 0) v))
             (:narrow (tag virtual final)
               (const c class (assert-in (nth env 0) class))
               (function (eval-initial-value) object-opt
                 (return ((eval :variable-initialisation) env run)))
               (const v instance-variable (new instance-variable :uninit (in member-mod (tag final)) :uninit eval-initial-value immutable))
               (action<- (overridden-var :variable-binding 0) (assert-in (define-instance-member c cxt name (& namespaces a) (& override-mod a) (& explicit a) v)
                                                                         instance-variable-opt))
               (action<- (compile-var :variable-binding 0) v))
             (:select (tag constructor)
               (throw definition-error))))))
        
        ((setup)
         (const env environment (compile-env :variable-binding 0))
         (const v (union dynamic-var variable instance-variable) (compile-var :variable-binding 0))
         (case v
           (:select dynamic-var
             ((setup :variable-initialisation)))
           (:narrow variable
             (const type class (get-variable-type v compile))
             (case (assert-not-in (& value v) (union (tag uninitialised) uninstantiated-function))
               (:select object)
               (:select (tag inaccessible) ((setup :variable-initialisation)))
               (:select (-> () object)
                 (&= value v inaccessible)
                 ((setup :variable-initialisation))
                 (catch ((const value object-opt ((eval :variable-initialisation) env compile))
                         (when (not-in value (tag none) :narrow-true)
                           (const coerced-value object ((&opt implicit-coerce type) value false))
                           (&= value v coerced-value)))
                   (x)
                   (rwhen (not-in x (tag compile-expression-error))
                     (throw x))
                   (note "If a " (:tag compile-expression-error) " occurred, then the initialiser is not a compile-time constant expression. "
                         "In this case, ignore the error and leave the value of the variable " (:tag inaccessible) " until it is defined at run time.")))))
           (:narrow instance-variable
             (var t class-opt ((setup-and-eval :typed-identifier) env))
             (when (in t (tag none))
               (const overridden-var instance-variable-opt (overridden-var :variable-binding 0))
               (if (not-in overridden-var (tag none) :narrow-true)
                 (<- t (&opt type overridden-var))
                 (<- t object-class)))
             (&const= type v (assert-not-in t (tag none)))
             ((setup :variable-initialisation)))))
        
        ((eval env immutable)
         (case (compile-var :variable-binding 0)
           (:select dynamic-var
             (const value object-opt ((eval :variable-initialisation) env run))
             (when (not-in value (tag none) :narrow-true)
               (lexical-write env (multiname :variable-binding 0) value false run)))
           (:select variable
             (const local-frame frame (nth env 0))
             (const members (list-set local-member) (map (& local-bindings local-frame) b (& content b) (set-in (& qname b) (multiname :variable-binding 0))))
             (note "The " (:local members) " set consists of exactly one " (:type variable) " element because " (:local local-frame)
                   " was constructed with that " (:type variable) " inside " (:action validate) ".")
             (const v variable (assert-in (unique-elt-of members) variable))
             (when (in (& value v) (tag inaccessible))
               (const value object-opt ((eval :variable-initialisation) env run))
               (const type class (get-variable-type v run))
               (var coerced-value object-u)
               (cond
                ((not-in value (tag none) :narrow-true) (<- coerced-value ((&opt implicit-coerce type) value false)))
                (immutable (<- coerced-value uninitialised))
                (nil (<- coerced-value (& default-value type))))
               (&= value v coerced-value)))
           (:select instance-variable)))))
    
    (rule (:variable-initialisation :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                            (eval (-> (environment phase) object-opt)))
      (production (:variable-initialisation :beta) () variable-initialisation-none
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return none)))
      (production (:variable-initialisation :beta) (= (:variable-initialiser :beta)) variable-initialisation-variable-initialiser
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :variable-initialiser) env phase)))))
    
    (rule (:variable-initialiser :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                         (eval (-> (environment phase) object)))
      (production (:variable-initialiser :beta) ((:assignment-expression :beta)) variable-initialiser-assignment-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (return (read-reference ((eval :assignment-expression) env phase) phase))))
      (production (:variable-initialiser :beta) (:nonexpression-attribute) variable-initialiser-nonexpression-attribute
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :nonexpression-attribute) env phase))))
      (production (:variable-initialiser :beta) (:attribute-combination) variable-initialiser-attribute-combination
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :attribute-combination) env phase)))))
    
    
    (rule (:typed-identifier :beta) ((name string) (plain boolean)
                                     (validate (-> (context environment) void))
                                     (setup-and-eval (-> (environment) class-opt)))
      (production (:typed-identifier :beta) (:identifier) typed-identifier-identifier
        (name (name :identifier))
        (plain true)
        ((validate (cxt :unused) (env :unused)))
        ((setup-and-eval (env :unused)) (return none)))
      (production (:typed-identifier :beta) (:identifier \: (:type-expression :beta)) typed-identifier-identifier-and-type
        (name (name :identifier))
        (plain false)
        ((validate cxt env) ((validate :type-expression) cxt env))
        ((setup-and-eval env) (return ((setup-and-eval :type-expression) env)))))
    ;(production (:typed-identifier :beta) ((:type-expression :beta) :identifier) typed-identifier-type-and-identifier)
    (%print-actions ("Validation" compile-env compile-var overridden-var multiname name plain immutable validate)
                    ("Setup" setup)
                    ("Evaluation" setup-and-eval eval))
    
    
    (%heading 2 "Simple Variable Definition")
    (%text :syntax "A " (:grammar-symbol :simple-variable-definition) " represents the subset of " (:grammar-symbol :variable-definition)
           " expansions that may be used when the variable definition is used as a " (:grammar-symbol (:substatement :omega))
           " instead of a " (:grammar-symbol (:directive :omega_2)) " in non-strict mode. "
           "In strict mode variable definitions may not be used as substatements.")
    (rule :simple-variable-definition ((validate (-> (context environment) void)) (setup (-> () void))
                                       (eval (-> (environment object) object)))
      (production :simple-variable-definition (var :untyped-variable-binding-list) simple-variable-definition-definition
        ((validate cxt env)
         (rwhen (or (& strict cxt) (not-in (get-regional-frame env) (union global-object parameter-frame)))
           (throw syntax-error))
         ((validate :untyped-variable-binding-list) cxt env))
        ((setup) ((setup :untyped-variable-binding-list)))
        ((eval env d)
         ((eval :untyped-variable-binding-list) env)
         (return d))))
    
    (rule :untyped-variable-binding-list ((validate (-> (context environment) void)) (setup (-> () void))
                                          (eval (-> (environment) void)))
      (production :untyped-variable-binding-list (:untyped-variable-binding) untyped-variable-binding-list-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env) ((eval :untyped-variable-binding) env)))
      (production :untyped-variable-binding-list (:untyped-variable-binding-list \, :untyped-variable-binding) untyped-variable-binding-list-more
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env)
         ((eval :untyped-variable-binding-list) env)
         ((eval :untyped-variable-binding) env))))
    
    (rule :untyped-variable-binding ((validate (-> (context environment) void)) (setup (-> () void))
                                     (eval (-> (environment) void)))
      (production :untyped-variable-binding (:identifier (:variable-initialisation allow-in)) untyped-variable-binding-full
        ((validate cxt env)
         ((validate :variable-initialisation) cxt env)
         (exec (define-hoisted-var env (name :identifier) undefined)))
        ((setup) ((setup :variable-initialisation)))
        ((eval env)
         (const value object-opt ((eval :variable-initialisation) env run))
         (when (not-in value (tag none) :narrow-true)
           (const qname qualified-name (new qualified-name public-namespace (name :identifier)))
           (lexical-write env (list-set qname) value false run)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Function Definition")
    (rule :function-definition ((enclosing-frame (writable-cell frame))
                                (overridden-method (writable-cell instance-method-opt))
                                (validate (-> (context environment plurality attribute-opt-not-false) void))
                                (setup (-> () void)))
      (production :function-definition (function :function-name :function-common) function-definition-definition
        ((validate cxt env pl attr)
         (const name string (name :function-name))
         (const kind function-kind (kind :function-name))
         (const a compound-attribute (to-compound-attribute attr))
         (rwhen (& dynamic a)
           (throw definition-error))
         (const unchecked boolean (and (not (& strict cxt)) (not-in (nth env 0) class) (in kind (tag normal)) (plain :function-common)))
         (const prototype boolean (or unchecked (& prototype a)))
         (var member-mod member-modifier (& member-mod a))
         (action<- (enclosing-frame :function-definition 0) (nth env 0))
         (cond
          ((in (nth env 0) class)
           (when (in member-mod (tag none))
             (<- member-mod virtual)))
          (nil
           (rwhen (not-in member-mod (tag none))
             (throw definition-error))))
         (rwhen (and prototype (or (not-in kind (tag normal)) (in member-mod (tag constructor))))
           (throw definition-error))
         (case member-mod
           (:select (tag none static)
             (var f (union simple-instance uninstantiated-function))
             (cond
              ((in kind (tag get set))
               (todo))
              (nil
               (const this (tag none inaccessible) (if prototype inaccessible none))
               (<- f ((validate-static-function :function-common) cxt env this unchecked prototype))))
             (when (in pl (tag singular))
               (<- f (instantiate-function (assert-in f uninstantiated-function) env)))
             (cond
              ((and unchecked
                    (in attr (tag none))
                    (or (in (nth env 0) global-object) (and (in (nth env 0) block-frame) (in (nth env 1) parameter-frame))))
               (exec (define-hoisted-var env name f)))
              (nil
               (const v variable (new variable function-class f true))
               (exec (define-local-member env name (& namespaces a) (& override-mod a) (& explicit a) read-write v))))
             (action<- (overridden-method :function-definition 0) none))
           (:narrow (tag virtual final)
             (assert (in pl (tag singular)))
             (rwhen (in kind (tag get set))
               (todo))
             ((validate :function-common) cxt env inaccessible false prototype)
             (const method instance-method (new instance-method :uninit (in member-mod (tag final)) (compile-frame :function-common) (eval-instance-call :function-common)))
             (action<- (overridden-method :function-definition 0)
                       (assert-in (define-instance-member (assert-in (nth env 0) class) cxt name (& namespaces a) (& override-mod a) (& explicit a) method)
                                  instance-method-opt)))
           (:select (tag constructor)
             (assert (in pl (tag singular)))
             (action<- (overridden-method :function-definition 0) none)
             (todo))))
        
        ((setup)
         (const overridden-method instance-method-opt (overridden-method :function-definition 0))
         (if (not-in overridden-method (tag none) :narrow-true)
           ((setup-override :function-common) (& signature overridden-method))
           ((setup :function-common))))))
    
    
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
    
    
    (rule :function-common ((plain boolean)
                            (compile-env (writable-cell environment))
                            (compile-frame (writable-cell parameter-frame))
                            (validate (-> (context environment (tag none inaccessible) boolean boolean) void))
                            (setup (-> () void))
                            (setup-override (-> (parameter-frame) void))
                            (eval-static-call (-> (object (vector object) environment phase) object))
                            (eval-instance-call (-> (object (vector object) phase) object))
                            (eval-prototype-construct (-> ((vector object) environment phase) object))
                            (validate-static-function (-> (context environment (tag none inaccessible) boolean boolean) uninstantiated-function)))
      (production :function-common (\( :parameters \) :result :block) function-common-signatures-and-block
        (plain (and (plain :parameters) (plain :result)))
        ((validate cxt env this unchecked prototype)
         (const compile-frame parameter-frame (new parameter-frame (list-set-of local-binding) plural this unchecked prototype
                                                   (vector-of parameter)
                                                   none :uninit))
         (const compile-env environment (cons compile-frame env))
         (action<- (compile-frame :function-common 0) compile-frame)
         (action<- (compile-env :function-common 0) compile-env)
         ((validate :parameters) cxt compile-env compile-frame)
         ((validate :result) cxt compile-env)
         ((validate :block) cxt compile-env (new jump-targets (list-set-of label) (list-set-of label)) plural))

        ((setup)
         (const compile-env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         ((setup :parameters) compile-env compile-frame)
         ((setup :result) compile-env compile-frame)
         ((setup :block)))

        ((setup-override overridden-signature)
         (const compile-env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         ((setup-override :parameters) compile-env compile-frame overridden-signature)
         ((setup-override :result) compile-env compile-frame overridden-signature)
         ((setup :block)))
        
        ((eval-static-call this args runtime-env phase)
         (rwhen (in phase (tag compile))
           (throw compile-expression-error))
         (var runtime-this object-opt none)
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (when (& prototype compile-frame)
           (const g (union package global-object) (get-package-or-global-frame runtime-env))
           (if (and (in this (tag null undefined)) (in g global-object :narrow-true))
             (<- runtime-this g)
             (<- runtime-this this)))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame runtime-env runtime-this))
         (assign-arguments runtime-frame args)
         (var result object)
         (catch ((exec ((eval :block) (cons runtime-frame runtime-env) undefined))
                 (<- result undefined))
           (x) (if (in x returned-value :narrow-true)
                 (<- result (& value x))
                 (throw x)))
         (const coerced-result object ((&opt implicit-coerce (&opt return-type runtime-frame)) result false))
         (return coerced-result))
        
        ((eval-instance-call (this :unused) (args :unused) (phase :unused))
         (todo))
        
        ((eval-prototype-construct (args :unused) (runtime-env :unused) (phase :unused))
         (todo))
        
        ((validate-static-function cxt env this unchecked prototype)
         ((validate :function-common 0) cxt env this unchecked prototype)
         (cond
          (prototype
           (todo))
          (nil
           (const initial-slots (list-set slot)
             (list-set (new slot
                         (assert-in (lookup-instance-member function-class (new qualified-name public-namespace "length") read) instance-variable)
                         (real-to-float64 (parameter-count :parameters)))))
           ;***** This would be better using a function that constructs the slots out of the Function type.
           (return (new uninstantiated-function function-class initial-slots false (eval-static-call :function-common 0) none (list-set-of simple-instance))))))))

    (%print-actions ("Validation" enclosing-frame overridden-method kind name plain compile-env compile-frame validate validate-static-function)
                    ("Setup" setup setup-override)
                    ("Evaluation" eval-static-call eval-prototype-construct))
    
    
    (define (assign-arguments (runtime-frame parameter-frame) (args (vector object))) void
      (var n integer 0)
      (// "This procedure performs a number of checks on the arguments, including checking their count, names, and values. "
          "Although this procedure performs these checks in a specific order for expository purposes, an implementation may perform these checks in a different "
          "order, which could have the effect of reporting a different error if there are multiple errors. "
          "For example, if a function only allows between 2 and 4 arguments, the first of which must be a " (:character-literal "Number")
          " and is passed five arguments the first of which is a " (:character-literal "String") ", then the implementation may throw an exception either about "
          "the argument count mismatch or about the type coercion error in the first argument.")
      (when (& unchecked runtime-frame)
        ;***** Create arguments array
        (todo))
      (for-each (&opt parameters runtime-frame) parameter
        (var argument object-opt)
        (cond
         ((= n (length args))
          (<- argument (& default parameter))
          (rwhen (in argument (tag none))
            (throw argument-mismatch-error)))
         (nil
          (<- argument (nth args n))
          (<- n (+ n 1))))
        (write-local-member (& var parameter) (assert-not-in argument (tag none)) run))
      (const rest (union variable (tag none)) (&opt rest runtime-frame))
      (cond
       ((in rest (tag none) :narrow-false)
        (rwhen (/= n (length args))
          (throw argument-mismatch-error)))
       (nil
        (todo))))
    
    
    (rule :parameters ((plain boolean) (parameter-count integer)
                       (validate (-> (context environment parameter-frame) void))
                       (setup (-> (environment parameter-frame) void))
                       (setup-override (-> (environment parameter-frame parameter-frame) void)))
      (production :parameters () parameters-none
        (plain true)
        (parameter-count 0)
        ((validate cxt env compile-frame) :forward)
        ((setup compile-env compile-frame) :forward)
        ((setup-override (compile-env :unused) (compile-frame :unused) overridden-signature)
         (rwhen (or (nonempty (&opt parameters overridden-signature))
                    (not-in (&opt rest overridden-signature) (tag none)))
           (throw definition-error))))
      (production :parameters (:nonempty-parameters) parameters-nonempty
        (plain (plain :nonempty-parameters))
        (parameter-count (parameter-count :nonempty-parameters))
        ((validate cxt env compile-frame) :forward)
        ((setup compile-env compile-frame) :forward)
        ((setup-override compile-env compile-frame overridden-signature)
         ((setup-override :nonempty-parameters) compile-env compile-frame overridden-signature (&opt parameters overridden-signature)))))
    
    
    (rule :nonempty-parameters ((plain boolean) (parameter-count integer)
                                (validate (-> (context environment parameter-frame) void))
                                (setup (-> (environment parameter-frame) void))
                                (setup-override (-> (environment parameter-frame parameter-frame (vector parameter)) void)))
      (production :nonempty-parameters (:parameter-init) nonempty-parameters-parameter-init
        (plain (plain :parameter-init))
        (parameter-count 1)
        ((validate cxt env compile-frame) :forward)
        ((setup compile-env compile-frame) :forward)
        ((setup-override compile-env compile-frame overridden-signature overridden-parameters)
         (rwhen (empty overridden-parameters)
           (throw definition-error))
         ((setup-override :parameter-init) compile-env compile-frame (nth overridden-parameters 0))
         (rwhen (or (/= (length overridden-parameters) 1)
                    (not-in (&opt rest overridden-signature) (tag none)))
           (throw definition-error))))
      (production :nonempty-parameters (:parameter-init \, :nonempty-parameters) nonempty-parameters-parameter-init-and-more
        (plain (and (plain :parameter-init) (plain :nonempty-parameters)))
        (parameter-count (+ 1 (parameter-count :nonempty-parameters)))
        ((validate cxt env compile-frame) :forward)
        ((setup compile-env compile-frame) :forward)
        ((setup-override compile-env compile-frame overridden-signature overridden-parameters)
         (rwhen (empty overridden-parameters)
           (throw definition-error))
         ((setup-override :parameter-init) compile-env compile-frame (nth overridden-parameters 0))
         ((setup-override :nonempty-parameters) compile-env compile-frame overridden-signature (subseq overridden-parameters 1))))
      (production :nonempty-parameters (:rest-parameter) nonempty-parameters-rest-parameter
        (plain false)
        (parameter-count 0)
        ((validate cxt env compile-frame) :forward)
        ((setup compile-env compile-frame) :forward)
        ((setup-override compile-env compile-frame overridden-signature overridden-parameters)
         (rwhen (nonempty overridden-parameters)
           (throw definition-error))
         (const overridden-rest (union variable (tag none)) (&opt rest overridden-signature))
         (rwhen (in overridden-rest (tag none) :narrow-false)
           (throw definition-error))
         ((setup-override :rest-parameter) compile-env compile-frame overridden-rest))))
    
    
    (rule :parameter-core ((plain boolean)
                           (compile-var (writable-cell (union dynamic-var variable)))
                           (validate (-> (context environment parameter-frame boolean) void))
                           (setup (-> (environment parameter-frame object-opt) void))
                           (setup-override (-> (environment parameter-frame object-opt parameter) void)))
      (production :parameter-core ((:typed-identifier allow-in)) parameter-core-typed-identifier
        (plain (plain :typed-identifier))
        ((validate cxt env compile-frame immutable)
         ((validate :typed-identifier) cxt env)
         (const name string (name :typed-identifier))
         (var v (union dynamic-var variable))
         (cond
          ((& unchecked compile-frame)
           (assert (not immutable))
           (<- v (define-hoisted-var env name undefined)))
          (nil
           (<- v (new variable inaccessible inaccessible immutable))
           (exec (define-local-member env name (list-set public-namespace) none false read-write v))))
         (action<- (compile-var :parameter-core 0) v))
        ((setup compile-env compile-frame default)
         (rwhen (and (in default (tag none)) (some (&opt parameters compile-frame) p2 (not-in (& default p2) (tag none))))
           (note "A required parameter cannot follow an optional one.")
           (throw definition-error))
         (const v (union dynamic-var variable) (compile-var :parameter-core 0))
         (case v
           (:select dynamic-var)
           (:narrow variable
             (var type class-opt ((setup-and-eval :typed-identifier) compile-env))
             (when (in type (tag none))
               (<- type object-class))
             (&= type v (assert-not-in type (tag none)))
             (&= value v uninitialised)))
         (const p parameter (new parameter v default))
         (&= parameters compile-frame (append (&opt parameters compile-frame) (vector p))))
        ((setup-override compile-env compile-frame default overridden-parameter)
         (var new-default object-opt default)
         (when (in new-default (tag none))
           (<- new-default (& default overridden-parameter)))
         (rwhen (and (in default (tag none)) (some (&opt parameters compile-frame) p2 (not-in (& default p2) (tag none))))
           (note "A required parameter cannot follow an optional one.")
           (throw definition-error))
         (const v (union dynamic-var variable) (compile-var :parameter-core 0))
         (assert (not-in v dynamic-var :narrow-true))
         (var type class-opt ((setup-and-eval :typed-identifier) compile-env))
         (when (in type (tag none))
           (<- type object-class))
         (rwhen (/= (assert-not-in type (tag none)) (get-variable-type (assert-not-in (& var overridden-parameter) dynamic-var) compile) class)
           (throw definition-error))
         (&= type v (assert-not-in type (tag none)))
         (&= value v uninitialised)
         (const p parameter (new parameter v new-default))
         (&= parameters compile-frame (append (&opt parameters compile-frame) (vector p))))))


    (rule :parameter ((plain boolean)
                      (compile-var (writable-cell (union dynamic-var variable)))
                      (validate (-> (context environment parameter-frame) void))
                      (setup (-> (environment parameter-frame object-opt) void))
                      (setup-override (-> (environment parameter-frame object-opt parameter) void)))
      (production :parameter (:parameter-core) parameter-parameter-core
        (plain (plain :parameter-core))
        ((validate cxt env compile-frame)
         ((validate :parameter-core) cxt env compile-frame false))
        ((setup compile-env compile-frame default) :forward)
        ((setup-override compile-env compile-frame default overridden-parameter) :forward))
      (production :parameter (const :parameter-core) parameter-const-parameter-core
        (plain false)
        ((validate cxt env compile-frame)
         ((validate :parameter-core) cxt env compile-frame true))
        ((setup compile-env compile-frame default) :forward)
        ((setup-override compile-env compile-frame default overridden-parameter) :forward)))
    
    
    (rule :parameter-init ((plain boolean)
                           (validate (-> (context environment parameter-frame) void))
                           (setup (-> (environment parameter-frame) void))
                           (setup-override (-> (environment parameter-frame parameter) void)))
      (production :parameter-init (:parameter) parameter-init-parameter
        (plain (plain :parameter))
        ((validate cxt env compile-frame)
         ((validate :parameter) cxt env compile-frame))
        ((setup compile-env compile-frame)
         ((setup :parameter) compile-env compile-frame none))
        ((setup-override compile-env compile-frame overridden-parameter)
         ((setup-override :parameter) compile-env compile-frame none overridden-parameter)))
      (production :parameter-init (:parameter = (:assignment-expression allow-in)) parameter-init-initialiser
        (plain false)
        ((validate cxt env compile-frame)
         ((validate :parameter) cxt env compile-frame)
         ((validate :assignment-expression) cxt env))
        ((setup compile-env compile-frame)
         ((setup :assignment-expression))
         (const default object (read-reference ((eval :assignment-expression) compile-env compile) compile))
         ((setup :parameter) compile-env compile-frame default))
        ((setup-override compile-env compile-frame overridden-parameter)
         ((setup :assignment-expression))
         (const default object (read-reference ((eval :assignment-expression) compile-env compile) compile))
         ((setup-override :parameter) compile-env compile-frame default overridden-parameter))))
    
    
    (rule :rest-parameter ((validate (-> (context environment parameter-frame) void))
                           (setup (-> (environment parameter-frame) void))
                           (setup-override (-> (environment parameter-frame variable) void)))
      (production :rest-parameter (\.\.\.) rest-parameter-none
        ((validate (cxt :unused) (env :unused) (compile-frame :unused)) (todo))
        ((setup (compile-env :unused) (compile-frame :unused)) (todo))
        ((setup-override (compile-env :unused) (compile-frame :unused) (overridden-rest :unused)) (todo)))
      (production :rest-parameter (\.\.\. :parameter) rest-parameter-parameter
        ((validate (cxt :unused) (env :unused) (compile-frame :unused)) (todo))
        ((setup (compile-env :unused) (compile-frame :unused)) (todo))
        ((setup-override (compile-env :unused) (compile-frame :unused) (overridden-rest :unused)) (todo))))
    
    
    (rule :result ((plain boolean)
                   (validate (-> (context environment) void))
                   (setup (-> (environment parameter-frame) void))
                   (setup-override (-> (environment parameter-frame parameter-frame) void)))
      (production :result () result-none
        (plain true)
        ((validate cxt env) :forward)
        ((setup (compile-env :unused) compile-frame)
         (&const= return-type compile-frame object-class))
        ((setup-override (compile-env :unused) compile-frame overridden-signature)
         (&const= return-type compile-frame (&opt return-type overridden-signature))))
      (production :result (\: (:type-expression allow-in)) result-colon-and-type-expression
        (plain false)
        ((validate cxt env) :forward)
        ((setup compile-env compile-frame)
         (&const= return-type compile-frame ((setup-and-eval :type-expression) compile-env)))
        ((setup-override compile-env compile-frame overridden-signature)
         (const t class ((setup-and-eval :type-expression) compile-env))
         (rwhen (/= (&opt return-type overridden-signature) t class)
           (throw definition-error))
         (&const= return-type compile-frame t)))
      ;(production :result ((:- {) (:type-expression allow-in)) result-type-expression)
      )
    (%print-actions ("Validation" plain parameter-count compile-var validate) ("Setup" setup setup-override))
    
    
    (%heading 2 "Class Definition")
    (rule :class-definition ((class (writable-cell class))
                             (validate (-> (context environment plurality attribute-opt-not-false) void))
                             (setup (-> () void))
                             (eval (-> (environment object) object)))
      (production :class-definition (class :identifier :inheritance :block) class-definition-definition
        ((validate cxt env pl attr)
         (rwhen (not-in pl (tag singular))
           (throw syntax-error))
         (const superclass class ((validate :inheritance) cxt env))
         (var a compound-attribute (to-compound-attribute attr))
         (rwhen (or (not (& complete superclass)) (& final superclass))
           (throw definition-error))
         (function (call (this object :unused) (args (vector object) :unused) (phase phase :unused)) object
           (todo))
         (function (construct (args (vector object) :unused) (phase phase :unused)) object
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
         (const c class (new class (list-set-of local-binding) superclass (list-set-of instance-member)
                             (vector-of instance-variable) false superclass prototype "object" private-namespace dynamic final
                             call construct :uninit :uninit null))
         (function (is-instance-of (o object)) boolean
           (return (or (in o (tag null)) (is-ancestor c (object-type o)))))
         (function (coerce (o object) (silent boolean)) object
           (cond
            (((&opt is-instance-of c) o) (return o))
            (silent (return null))
            (nil (throw bad-value-error))))
         (&const= is-instance-of c is-instance-of)
         (&const= implicit-coerce c coerce)
         (action<- (class :class-definition 0) c)
         (const v variable (new variable class-class c true))
         (exec (define-local-member env (name :identifier) (& namespaces a) (& override-mod a) (& explicit a) read-write v))
         ((validate-using-frame :block) cxt env (new jump-targets (list-set-of label) (list-set-of label)) pl c)
         (&= complete c true))
        
        ((setup)
         ((setup :block)))
        
        ((eval env d)
         (const c class (class :class-definition 0))
         (return ((eval-using-frame :block) env c d)))))
    
    
    (rule :inheritance ((validate (-> (context environment) class)))
      (production :inheritance () inheritance-none
        ((validate (cxt :unused) (env :unused)) (return object-class)))
      (production :inheritance (extends (:type-expression allow-in)) inheritance-extends
        ((validate cxt env)
         ((validate :type-expression) cxt env)
         (return ((setup-and-eval :type-expression) env))))
      #|(production :inheritance (implements :type-expression-list) inheritance-implements
        ((validate (cxt :unused) (env :unused)) (return object-class)))
      (production :inheritance (extends (:type-expression allow-in) implements :type-expression-list) inheritance-extends-implements
        ((validate (cxt :unused) (env :unused)) (return object-class)))|#)
    (%print-actions ("Validation" class validate) ("Setup" setup) ("Evaluation" eval))
    
    
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
         (exec (define-local-member env name (& namespaces a) (& override-mod a) (& explicit a) read-write v)))))
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
          (exec ((validate :directives) initial-context initial-environment (new jump-targets (list-set-of label) (list-set-of label)) singular none))
          ((setup :directives))
          (return ((eval :directives) initial-environment undefined))))))
    (%print-actions ("Evaluation" eval-program))
    
    
    
    (%heading (1 :semantics) "Predefined Identifiers")
    
    
    (%heading (1 :semantics) "Built-in Classes")
    (define (make-built-in-class (superclass class-opt) (typeof-string string) (dynamic boolean) (allow-null boolean) (final boolean) (default-value object)) class
      (function (call (this object :unused) (args (vector object) :unused) (phase phase :unused)) object
        (todo))
      (function (construct (args (vector object) :unused) (phase phase :unused)) object
        (todo))
      (function (implicit-coerce (o object :unused) (silent boolean :unused)) object
        (todo))
      (const private-namespace namespace (new namespace "private"))
      (const c class (new class (list-set-of local-binding) superclass (list-set-of instance-member)
                          (vector-of instance-variable) true superclass null typeof-string private-namespace dynamic final
                          call construct :uninit implicit-coerce default-value))
      (function (is-instance-of (o object)) boolean
        (return (or (is-ancestor c (object-type o))
                    (and allow-null (in o (tag null))))))
      (&const= is-instance-of c is-instance-of)
      (return c))
    
    (define (make-built-in-integer-class (low integer) (high integer)) class
      (function (call (this object :unused) (args (vector object) :unused) (phase phase :unused)) object
        (todo))
      (function (construct (args (vector object) :unused) (phase phase :unused)) object
        (todo))
      (function (is-instance-of (o object)) boolean
        (if (in o float64 :narrow-true)
          (case o
            (:select (tag nan64 +infinity64 -infinity64) (return false))
            (:select (tag +zero64 -zero64) (return true))
            (:narrow nonzero-finite-float64
              (const r rational (& value o))
              (if (and (in r integer :narrow-true) (cascade integer low <= r <= high))
                (return true)
                (return false))))
          (return false)))
      (function (implicit-coerce (o object) (silent boolean :unused)) object
        (cond
         ((in o (tag undefined) :narrow-false) (return +zero64))
         ((in o general-number :narrow-true)
          (const i integer-opt (check-integer o))
          (when (and (not-in i (tag none) :narrow-true) (cascade integer low <= i <= high))
            (note (:tag -zero32) ", " (:tag +zero32) ", and " (:tag -zero64) " are all coerced to " (:tag +zero64) ".")
            (return (real-to-float64 i)))))
        (throw bad-value-error))
      (const private-namespace namespace (new namespace "private"))
      (return (new class (list-set-of local-binding) number-class (list-set-of instance-member)
                   (vector-of instance-variable) true number-class null "number" private-namespace false true
                   call construct is-instance-of implicit-coerce +zero64)))
    
    (define object-class class (make-built-in-class none "object" false true false undefined))
    (define undefined-class class (make-built-in-class object-class "undefined" false false true undefined))
    (define null-class class (make-built-in-class object-class "object" false true true null))
    (define boolean-class class (make-built-in-class object-class "boolean" false false true false))
    (define general-number-class class (make-built-in-class object-class "object" false false false nan64))
    (define long-class class (make-built-in-class general-number-class "long" false false true (new long 0)))
    (define u-long-class class (make-built-in-class general-number-class "ulong" false false true (new u-long 0)))
    (define float-class class (make-built-in-class general-number-class "float" false false true nan32))
    (define number-class class (make-built-in-class general-number-class "number" false false true nan64))
    (define s-byte-class class (make-built-in-integer-class -128 127))
    (define byte-class class (make-built-in-integer-class 0 255))
    (define short-class class (make-built-in-integer-class -32768 32767))
    (define u-short-class class (make-built-in-integer-class 0 65535))
    (define int-class class (make-built-in-integer-class -2147483648 2147483647))
    (define u-int-class class (make-built-in-integer-class 0 4294967295))
    (define character-class class (make-built-in-class object-class "character" false false true #?0000))
    (define string-class class (make-built-in-class object-class "string" false true true null))
    (define namespace-class class (make-built-in-class object-class "namespace" false true true null))
    (define attribute-class class (make-built-in-class object-class "object" false true true null))
    (define date-class class (make-built-in-class object-class "object" true true true null))
    (define reg-exp-class class (make-built-in-class object-class "object" true true true null))
    (define class-class class (make-built-in-class object-class "function" false true true null))
    (define function-class class (make-built-in-class object-class "function" false true true null))
    (define prototype-class class (make-built-in-class object-class "object" true true true null))
    (define package-class class (make-built-in-class object-class "object" true true true null))
    
    
    (define object-prototype simple-instance (new simple-instance (list-set-of local-binding) none false prototype-class (list-set-of slot) none none none none))
    ;***** Add some properties here
    
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
