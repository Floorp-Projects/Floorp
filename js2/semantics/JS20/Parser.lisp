;;;
;;; JavaScript 2.0 parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(defparameter *jw-source*
  '((line-grammar code-grammar :lr-1 :program)
    
    (%heading (1 :semantics) "Data Model")
    (%heading (2 :semantics) "Semantic Exceptions")
    
    (deftuple break (value object) (label label))
    (deftuple continue (value object) (label label))
    (deftuple return (value object))
    
    (deftype control-transfer (union break continue return))
    (deftype semantic-exception (union object control-transfer))
    
    (%heading (2 :semantics) "Extended integers and rationals")
    
    (deftag +zero)
    (deftag -zero)
    (deftag +infinity)
    (deftag -infinity)
    (deftag nan)
    (deftype extended-rational (union (exclude-zero rational) (tag +zero -zero +infinity -infinity nan)))
    (deftype extended-integer (union integer (tag +infinity -infinity nan)))
    (? hide
      (deftag syntax-error))
    
    
    (%heading (2 :semantics) "Objects")
    (deftag none)
    (deftag ok)
    (deftag reject)
    
    (deftype object (union undefined null boolean long u-long float32 float64 char16 string namespace compound-attribute
                           class simple-instance method-closure date reg-exp package))
    (deftype primitive-object (union undefined null boolean long u-long float32 float64 char16 string))
    (deftype nonprimitive-object (union namespace compound-attribute class simple-instance method-closure date reg-exp package))
    (deftype binding-object (union class simple-instance reg-exp date package))
    
    (deftype object-opt (union object (tag none)))
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
    
    
    (%heading (4 :semantics) "Qualified Names")
    (deftuple qualified-name (namespace namespace) (id string))
    (definfix qualified-name ("::") ns id)
    
    (deftype multiname (list-set qualified-name))
    
    
    (%heading (3 :semantics) "Attributes")
    (deftag static)
    (deftag virtual)
    (deftag final)
    (deftype property-category (tag none static virtual final))
    
    (deftype override-modifier (tag none true false undefined))
    
    (deftuple compound-attribute
      (namespaces (list-set namespace))
      (explicit boolean)
      (enumerable boolean)
      (dynamic boolean)
      (category property-category)
      (override-mod override-modifier)
      (prototype boolean)
      (unused boolean))
    
    (deftype attribute (union boolean namespace compound-attribute))
    (deftype attribute-opt-not-false (union (tag none true) namespace compound-attribute))
    
    
    (%heading (3 :semantics) "Classes")
    (defrecord class
      (local-bindings (list-set local-binding) :var)
      (instance-properties (list-set instance-property) :var)
      (super class-opt)
      (prototype object-opt :opt-const)
      (complete boolean :var)
      (name string)
      (typeof-string string)
      (private-namespace namespace :opt-const)
      (dynamic boolean)
      (final boolean)
      (default-value object-opt)
      (default-hint hint :opt-const)
      (has-property (-> (object class object boolean phase) boolean))
      (bracket-read (-> (object class (vector object) boolean phase) object-opt))
      (bracket-write (-> (object class (vector object) object boolean (tag run)) (tag none ok)))
      (bracket-delete (-> (object class (vector object) (tag run)) boolean-opt))
      (read (-> (object class multiname environment-opt boolean phase) object-opt))
      (write (-> (object class multiname environment-opt object boolean (tag run)) (tag none ok)))
      (delete (-> (object class multiname environment-opt (tag run)) boolean-opt))
      (enumerate (-> (object) (list-set object)))
      (call (-> (object class (vector object) phase) object))
      (construct (-> (class (vector object) phase) object))
      (init (union (-> (simple-instance (vector object) (tag run)) void) (tag none)) :var)
      (is (-> (object class) boolean))
      (coerce (-> (object class) object-opt)))
    (deftype class-opt (union class (tag none)))
    
    
    (%heading (3 :semantics) "Simple Instances")
    (defrecord simple-instance
      (local-bindings (list-set local-binding) :var)
      (archetype object-opt :opt-const)
      (sealed boolean :var)
      (type class)
      (slots (list-set slot))
      (call (union (-> (object simple-instance (vector object) phase) object) (tag none)))
      (construct (union (-> (simple-instance (vector object) phase) object) (tag none)))
      (env environment-opt))
    
    
    (%heading (4 :semantics) "Slots")
    (defrecord slot
      (id instance-variable)
      (value object-opt :var))
    
    
    (%heading (3 :semantics) "Uninstantiated Functions")
    (defrecord uninstantiated-function
      (type class)
      (length integer)
      (call (union (-> (object simple-instance (vector object) phase) object) (tag none)))
      (construct (union (-> (simple-instance (vector object) phase) object) (tag none)))
      (instantiations (list-set simple-instance) :var))
    
    
    (%heading (3 :semantics) "Method Closures")
    (deftuple method-closure
      (this object)
      (method instance-method)
      (slots (list-set slot)))
    
    
    (%heading (3 :semantics) "Dates")
    (defrecord date
      (local-bindings (list-set local-binding) :var)
      (archetype object-opt)
      (sealed boolean :var)
      (time-value integer))
    
    
    (%heading (3 :semantics) "Regular Expressions")
    (defrecord reg-exp
      (local-bindings (list-set local-binding) :var)
      (archetype object-opt)
      (sealed boolean :var)
      (source string)
      (last-index integer)
      (global boolean)
      (ignore-case boolean)
      (multiline boolean))
    
   
    (%heading (3 :semantics) "Packages")
    (defrecord package
      (local-bindings (list-set local-binding) :var)
      (archetype object-opt)
      (name string)
      (initialize (union (-> () void) (tag none busy)) :var)
      (sealed boolean :var)
      (internal-namespace namespace))
    
    
    (%heading (2 :semantics) "Objects with Limits")
    (%text :comment (:label limited-instance instance) " must be an instance of one of "
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
      (base object)
      (limit class)
      (multiname multiname))
    
    (deftuple bracket-reference
      (base object)
      (limit class)
      (args (vector object)))
    
    (deftype reference (union lexical-reference dot-reference bracket-reference))
    (deftype obj-or-ref (union object reference))
    
    
    (%heading (2 :semantics) "Modes of expression evaluation")
    (deftag compile)
    (deftag run)
    (deftype phase (tag compile run))
    
    
    (%heading (2 :semantics) "Contexts")
    (defrecord context
      (strict boolean :var)
      (open-namespaces (list-set namespace) :var))
    
    
    (%heading (2 :semantics) "Labels")
    (deftag default)
    (deftype label (union string (tag default)))
    
    (deftuple jump-targets
      (break-targets (list-set label))
      (continue-targets (list-set label)))
    
    
    (%heading (2 :semantics) "Function Support")
    (deftag normal)
    (deftag get)
    (deftag set)
    (deftype handling (tag normal get set))
    
    (deftag plain-function)
    (deftag unchecked-function)
    (deftag prototype-function)
    (deftag instance-function)
    (deftag constructor-function)
    (deftype static-function-kind (tag plain-function unchecked-function prototype-function))
    (deftype function-kind (tag plain-function unchecked-function prototype-function instance-function constructor-function))
    
    
    (%heading (2 :semantics) "Environments")
    (%text :comment "An " (:type environment) " is a list of two or more frames. Each frame corresponds to a scope. "
           "More specific frames are listed first" :m-dash "each frame" :apostrophe "s scope is directly contained in the following frame"
           :apostrophe "s scope. The last frame is always a " (:type package)
           ". A " (:type with-frame) " is always preceded by a " (:type local-frame) ", so the first frame is never a " (:type with-frame) ".")
    (deftype environment (vector frame))
    (deftype environment-opt (union environment (tag none)))
    (deftype frame (union non-with-frame with-frame))
    (deftype non-with-frame (union package parameter-frame class local-frame))
    
    (defrecord parameter-frame
      (local-bindings (list-set local-binding) :var)
      (kind function-kind)
      (handling handling)
      (calls-superconstructor boolean :var)
      (superconstructor-called boolean :var)
      (this object-opt)
      (parameters (vector parameter) :opt-var)
      (rest variable-opt :opt-var)
      (return-type class :opt-const))
    (deftype parameter-frame-opt (union parameter-frame (tag none)))
     
    (deftuple parameter
      (var (union variable dynamic-var))
      (default object-opt))
    
    (defrecord local-frame
      (local-bindings (list-set local-binding) :var))
    
    (defrecord with-frame
      (value object-opt))
    
    
    (%heading (3 :semantics) "Properties")
    (deftag read)
    (deftag write)
    (deftag read-write)
    (deftype access (tag read write))
    (deftype access-set (tag read write read-write))
    
    
    (deftuple local-binding 
      (qname qualified-name)
      (accesses access-set)
      (explicit boolean)
      (enumerable boolean)
      (content singleton-property))
    
    
    (deftag forbidden)
    (deftype singleton-property (union (tag forbidden) variable dynamic-var getter setter))
    (deftype singleton-property-opt (union singleton-property (tag none)))
   
    (deftype variable-value (union (tag none) object uninstantiated-function))
    
    (deftag busy)
    (deftype initializer (-> (environment phase) object))
    (deftype initializer-opt (union initializer (tag none)))
    
    (defrecord variable
      (type class :opt-const)
      (value variable-value :var)
      (immutable boolean)
      (setup (union (-> () class-opt) (tag none busy)) :var)
      (initializer (union initializer (tag none busy)) :var)
      (initializer-env environment :opt-const))
    (deftype variable-opt (union variable (tag none)))
    
    (defrecord dynamic-var
      (value (union object uninstantiated-function) :var)
      (sealed boolean :var))
    
    (defrecord getter
      (call (-> (environment phase) object))
      (env environment-opt))
    
    (defrecord setter
      (call (-> (object environment phase) void))
      (env environment-opt))
    
    
    (deftype instance-property (union instance-variable instance-method instance-getter instance-setter))
    (deftype instance-property-opt (union instance-property (tag none)))
    
    (defrecord instance-variable
      (multiname multiname :opt-const)
      (final boolean)
      (enumerable boolean :opt-const)
      (type class :opt-const)
      (default-value object-opt :opt-const)
      (immutable boolean))
    (deftype instance-variable-opt (union instance-variable (tag none)))
    
    (defrecord instance-method
      (multiname multiname :opt-const)
      (final boolean)
      (enumerable boolean :opt-const)
      (signature parameter-frame :opt-const)
      (length integer)
      (call (-> (object (vector object) phase) object)))
    
    (defrecord instance-getter
      (multiname multiname :opt-const)
      (final boolean)
      (enumerable boolean :opt-const)
      (signature parameter-frame :opt-const)
      (call (-> (object phase) object)))
    
    (defrecord instance-setter
      (multiname multiname :opt-const)
      (final boolean)
      (enumerable boolean :opt-const)
      (signature parameter-frame :opt-const)
      (call (-> (object object phase) void)))
    
    
    (deftype property-opt (union singleton-property instance-property (tag none)))
    
    
    (%heading (2 :semantics) "Miscellaneous")
    (deftag hint-string)
    (deftag hint-number)
    (deftype hint (tag hint-string hint-number))
    (deftype hint-opt (union hint (tag none)))
    
    (deftag less)
    (deftag equal)
    (deftag greater)
    (deftag unordered)
    (deftype order (tag less equal greater unordered))
    
    
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
    
    #|
    (%text :comment (:global-call truncate-to-extended-integer x) " returns " (:local x) " converted to an integer by rounding towards zero. If " (:local x)
           " is an infinity or a NaN, the result is " (:tag +infinity) ", " (:tag -infinity) ", or " (:tag nan) ", as appropriate.")
    (define (truncate-to-extended-integer (x general-number)) extended-integer
      (case x
        (:select (tag +infinity32 +infinity64) (return +infinity))
        (:select (tag -infinity32 -infinity64) (return -infinity))
        (:select (tag nan32 nan64) (return nan))
        (:narrow finite-float32 (return (truncate-finite-float32 x)))
        (:narrow finite-float64 (return (truncate-finite-float64 x)))
        (:narrow (union long u-long) (return (& value x)))))
    |#
    
    (%text :comment (:global-call truncate-to-integer x) " returns " (:local x) " converted to an integer by rounding towards zero. If " (:local x)
           " is an infinity or a NaN, the result is 0.")
    (define (truncate-to-integer (x general-number)) integer
      (case x
        (:select (tag nan32 nan64 +infinity32 +infinity64 -infinity32 -infinity64) (return 0))
        (:narrow finite-float32 (return (truncate-finite-float32 x)))
        (:narrow finite-float64 (return (truncate-finite-float64 x)))
        (:narrow (union long u-long) (return (& value x)))))
    
    
    (%text :comment (:def-const limit integer) (:global-call pin-extended-integer i limit negative-from-end) " returns " (:local i) " pinned to the set "
           (:expr (range-set integer) (range-set-of-ranges integer 0 limit)) ", where " (:local limit) " is a nonnegative integer. If "
           (:local negative-from-end) " is " (:tag true)
           ", then negative values of " (:local i) " from " (:expr integer (neg limit)) " through " (:expr integer -1) " are treated as 0 through "
           (:expr integer (- limit 1)) " respectively.")
    (define (pin-extended-integer (i extended-integer) (limit integer) (negative-from-end boolean)) integer
      (case i
        (:select (tag nan) (throw-error -range-error))
        (:select (tag -infinity) (return 0))
        (:select (tag +infinity) (return limit))
        (:narrow integer
          (var j integer i)
          (when (> j limit)
            (<- j limit))
          (when (and negative-from-end (< j 0))
            (<- j (+ j limit)))
          (when (< j 0)
            (<- j 0))
          (assert (cascade integer 0 <= j <= limit))
          (return j))))

    
    (%text :comment (:global-call check-integer x) " returns " (:local x) " converted to an integer if its mathematical value is, in fact, an integer. "
           "If " (:local x) " is an infinity or a NaN or has a fractional part, the result is " (:tag none) ".")
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
    
    (%text :comment (:global-call integer-to-long i) " converts " (:local i) " to the first of the types " (:type long) ", " (:type u-long) ", or " (:type float64)
           " that can contain the value " (:local i) ". If necessary, the " (:type float64) " result may be rounded or converted to an infinity using the IEEE 754 "
           :left-double-quote "round to nearest" :right-double-quote " mode.")
    (define (integer-to-long (i integer)) general-number
      (cond
       ((cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 63) 1))
        (return (new long i)))
       ((cascade integer (expt 2 63) <= i <= (- (expt 2 64) 1))
        (return (new u-long i)))
       (nil
        (return (real-to-float64 i)))))
    
    (%text :comment (:global-call integer-to-u-long i) " converts " (:local i) " to the first of the types " (:type u-long) ", " (:type long) ", or " (:type float64)
           " that can contain the value " (:local i) ". If necessary, the " (:type float64) " result may be rounded or converted to an infinity using the IEEE 754 "
           :left-double-quote "round to nearest" :right-double-quote " mode.")
    (define (integer-to-u-long (i integer)) general-number
      (cond
       ((cascade integer 0 <= i <= (- (expt 2 64) 1))
        (return (new u-long i)))
       ((cascade integer (neg (expt 2 63)) <= i <= -1)
        (return (new long i)))
       (nil
        (return (real-to-float64 i)))))
    
    (%text :comment (:global-call rational-to-long q) " converts " (:local q) " to one of the types " (:type long) ", " (:type u-long) ", or " (:type float64)
           ", whichever one can come the closest to representing the true value of " (:local q) ". If several of these types can come equally close to the value of "
           (:local q) ", then one of them is chosen according to the algorithm below.")
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
    
    (%text :comment (:global-call rational-to-u-long q) " converts " (:local q) " to one of the types " (:type u-long) ", " (:type long) ", or " (:type float64)
           ", whichever one can come the closest to representing the true value of " (:local q) ". If several of these types can come equally close to the value of "
           (:local q) ", then one of them is chosen according to the algorithm below.")
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
    
    
    (define (extended-rational-to-float32 (q extended-rational)) float32
      (case q
        (:narrow rational (return (real-to-float32 q)))
        (:select (tag +zero) (return +zero32))
        (:select (tag -zero) (return -zero32))
        (:select (tag +infinity) (return +infinity32))
        (:select (tag -infinity) (return -infinity32))
        (:select (tag nan) (return nan32))))
    
    
    (define (extended-rational-to-float64 (q extended-rational)) float64
      (case q
        (:narrow rational (return (real-to-float64 q)))
        (:select (tag +zero) (return +zero64))
        (:select (tag -zero) (return -zero64))
        (:select (tag +infinity) (return +infinity64))
        (:select (tag -infinity) (return -infinity64))
        (:select (tag nan) (return nan64))))
    
    
    (%text :comment (:global-call to-rational x) " returns the exact " (:type rational) " value of " (:local x) ".")
    (define (to-rational (x finite-general-number)) rational
      (case x
        (:select (tag +zero32 +zero64 -zero32 -zero64) (return 0))
        (:narrow (union nonzero-finite-float32 nonzero-finite-float64 long u-long) (return (& value x)))))
    
    (%text :comment (:global-call to-float32 x) " converts " (:local x) " to a " (:type float32) ", using the IEEE 754 "
           :left-double-quote "round to nearest" :right-double-quote " mode.")
    (define (to-float32 (x general-number)) float32
      (case x
        (:narrow (union long u-long) (return (real-to-float32 (& value x))))
        (:narrow float32 (return x))
        (:select (tag -infinity64) (return -infinity32))
        (:select (tag -zero64) (return -zero32))
        (:select (tag +zero64) (return +zero32))
        (:select (tag +infinity64) (return +infinity32))
        (:select (tag nan64) (return nan32))
        (:narrow nonzero-finite-float64 (return (real-to-float32 (& value x))))))
    
    (%text :comment (:global-call to-float64 x) " converts " (:local x) " to a " (:type float64) ", using the IEEE 754 "
           :left-double-quote "round to nearest" :right-double-quote " mode.")
    (define (to-float64 (x general-number)) float64
      (case x
        (:narrow (union long u-long) (return (real-to-float64 (& value x))))
        (:narrow float32 (return (float32-to-float64 x)))
        (:narrow float64 (return x))))
    
    
    (%text :comment (:global-call general-number-compare x y) " compares " (:local x) " with " (:local y) " using the IEEE 754 rules and returns "
           (:tag less) " if " (:local x) "<" (:local y) ", " (:tag equal) " if " (:local x) "=" (:local y) ", " (:tag greater) " if " (:local x) ">" (:local y)
           ", or " (:tag unordered) " if either " (:local x) " or " (:local y) " is a NaN. The comparison is done using the exact values of " (:local x) " and " (:local y)
           ", even if they have different types. Positive infinities compare equal to each other and greater than any other non-NaN values. Negative infinities compare "
           "equal to each other and less than any other non-NaN values. Positive and negative zeroes compare equal to each other.")
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
    
    
    (%heading (2 :semantics) "Character Utilities")
    
    (define (integer-to-u-t-f16 (i (integer-range 0 (hex #x10FFFF)))) string
      (cond
       ((cascade integer 0 <= i <= (hex #xFFFF))
        (return (vector (integer-to-char16 i))))
       (nil
        (const j (integer-range 0 (hex #xFFFFF)) (- i (hex #x10000)))
        (const high char16 (integer-to-char16 (+ (hex #xD800) (bitwise-shift j -10))))
        (const low char16 (integer-to-char16 (+ (hex #xDC00) (bitwise-and j (hex #x3FF)))))
        (return (vector high low)))))
    
    
    (define (char21-to-u-t-f16 (ch char21)) string
      (return (integer-to-u-t-f16 (char21-to-integer ch))))
    
    
    (define (surrogate-pair-to-supplementary-char (h char16) (l char16)) supplementary-char
      (const code-point (integer-range (hex #x10000) (hex #x10FFFF))
        (+ (+ (hex #x10000) (* (- (char16-to-integer h) (hex #xD800)) (hex #x400))) (- (char16-to-integer l) (hex #xDC00))))
      (return (integer-to-supplementary-char code-point)))
    
    
    (define (string-to-u-t-f32 (s string)) (vector char21)
      (var i integer 0)
      (var result (vector char21) (vector-of char21))
      (while (/= i (length s))
        (var ch char21)
        (cond
         ((and (set-in (nth s i) (range-set-of-ranges char16 #?D800 #?DBFF)) (/= (+ i 1) (length s)) (set-in (nth s (+ i 1)) (range-set-of-ranges char16 #?DC00 #?DFFF)))
          (<- ch (surrogate-pair-to-supplementary-char (nth s i) (nth s (+ i 1))))
          (<- i (+ i 2)))
         (nil
          (<- ch (nth s i))
          (<- i (+ i 1))))
        (<- result (append result (vector ch))))
      (return result))
    
    
    (define (char-to-lower-full (ch char21)) string
      (/* (:keyword return) " " (:local ch) " converted to a lower case character using the Unicode full, locale-independent case mapping. "
          "A single character may be converted to multiple characters. If " (:local ch) " has no lower case equivalent, then the result is the string "
          (:expr string (char21-to-u-t-f16 ch)) ".")
      (cond
       ((in ch supplementary-char :narrow-both)
        (return (char21-to-u-t-f16 ch))) ;Don't worry for now about converting the case of these
       (nil (return (vector (lisp-call char-downcase (ch) char16))))))
    
    
    (define (char-to-lower-localized (ch char21)) string
      (/* (:keyword return) " " (:local ch) " converted to a lower case character using the Unicode full case mapping in the host environment" :apostrophe
          "s current locale. "
          "A single character may be converted to multiple characters. If " (:local ch) " has no lower case equivalent, then the result is the string "
          (:expr string (char21-to-u-t-f16 ch)) ".")
      (cond
       ((in ch supplementary-char :narrow-both)
        (return (char21-to-u-t-f16 ch))) ;Don't worry for now about converting the case of these
       (nil (return (vector (lisp-call char-downcase (ch) char16))))))
    
    
    (define (char-to-upper-full (ch char21)) string
      (/* (:keyword return) " " (:local ch) " converted to a upper case character using the Unicode full, locale-independent case mapping. "
          "A single character may be converted to multiple characters. If " (:local ch) " has no upper case equivalent, then the result is the string "
          (:expr string (char21-to-u-t-f16 ch)) ".")
      (cond
       ((in ch supplementary-char :narrow-both)
        (return (char21-to-u-t-f16 ch))) ;Don't worry for now about converting the case of these
       (nil (return (vector (lisp-call char-upcase (ch) char16))))))
    
    
    (define (char-to-upper-localized (ch char21)) string
      (/* (:keyword return) " " (:local ch) " converted to a upper case character using the Unicode full case mapping in the host environment" :apostrophe
          "s current locale. "
          "A single character may be converted to multiple characters. If " (:local ch) " has no upper case equivalent, then the result is the string "
          (:expr string (char21-to-u-t-f16 ch)) ".")
      (cond
       ((in ch supplementary-char :narrow-both)
        (return (char21-to-u-t-f16 ch))) ;Don't worry for now about converting the case of these
       (nil (return (vector (lisp-call char-downcase (ch) char16))))))
    
    
    (%heading (2 :semantics) "Object Utilities")
    (%heading (3 :semantics) "Object Class Inquiries")
    (%text :comment (:global-call object-type o) " returns an " (:type object) " " (:local o) :apostrophe "s most specific type. Although "
           (:global object-type) " is used internally throughout this specification, in order to allow one programmer-visible class to be implemented as an "
           "ensemble of implementation-specific classes, no way is provided for a user program to directly obtain the result of calling " (:global object-type)
           " on an object.")
    (define (object-type (o object)) class
      (case o
        (:select undefined (return -void))
        (:select null (return -null))
        (:select boolean (return -boolean))
        (:select long (return \#long))
        (:select u-long (return ulong))
        (:select float32 (return float))
        (:select float64 (return -number))
        (:select char16 (return char))
        (:select string (return -string))
        (:select namespace (return -namespace))
        (:select compound-attribute (return -attribute))
        (:select class (return -class))
        (:narrow simple-instance (return (& type o)))
        (:select method-closure (return -function))
        (:select date (return -date))
        (:select reg-exp (return -reg-exp))
        (:select package (return -package))))
    
    
    (%text :comment (:global-call is o c) " returns " (:tag true) " if " (:local o) " is an instance of class " (:local c) " or one of its subclasses.")
    (define (is (o object) (c class)) boolean
      (return ((& is c) o c)))
    
    
    (%text :comment (:global-call ordinary-is o c) " is the implementation of " (:global is) " for a native class unless specified otherwise in the class" :apostrophe
           "s definition. Host classes may either also use " (:global ordinary-is) " or define a different procedure to perform this test.")
    (define (ordinary-is (o object) (c class)) boolean
      (return (is-ancestor c (object-type o))))
    
    
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
    
    
    (%heading (3 :semantics) "Object to Boolean Conversion")
    (%text :comment (:global-call object-to-boolean o) " returns " (:local o) " converted to a " (:global -boolean) ".")
    (define (object-to-boolean (o object)) boolean
      (case o
        (:select (union undefined null) (return false))
        (:narrow boolean (return o))
        (:narrow (union long u-long) (return (/= (& value o) 0)))
        (:narrow float32 (return (not-in o (tag +zero32 -zero32 nan32))))
        (:narrow float64 (return (not-in o (tag +zero64 -zero64 nan64))))
        (:narrow string (return (/= o "" string)))
        (:select (union char16 namespace compound-attribute class simple-instance method-closure date reg-exp package) (return true))))
    
    
    (%heading (3 :semantics) "Object to Primitive Conversion")
    (define (object-to-primitive (o object) (hint hint-opt) (phase phase)) primitive-object
      (rwhen (in o primitive-object :narrow-true)
        (return o))
      (const c class (object-type o))
      (var h hint)
      (if (in hint hint :narrow-true)
        (<- h hint)
        (<- h (&opt default-hint c)))
      (case h
        (:select (tag hint-string)
          (const to-string-method object-opt ((& read c) o c (list-set (new qualified-name public "toString")) none false phase))
          (when (not-in to-string-method (tag none) :narrow-true)
            (const r object (call o to-string-method (vector-of object) phase))
            (rwhen (in r primitive-object :narrow-true)
              (return r)))
          (const value-of-method object-opt ((& read c) o c (list-set (new qualified-name public "valueOf")) none false phase))
          (when (not-in value-of-method (tag none) :narrow-true)
            (const r object (call o value-of-method (vector-of object) phase))
            (rwhen (in r primitive-object :narrow-true)
              (return r))))
        (:select (tag hint-number)
          (const value-of-method object-opt ((& read c) o c (list-set (new qualified-name public "valueOf")) none false phase))
          (when (not-in value-of-method (tag none) :narrow-true)
            (const r object (call o value-of-method (vector-of object) phase))
            (rwhen (in r primitive-object :narrow-true)
              (return r)))
          (const to-string-method object-opt ((& read c) o c (list-set (new qualified-name public "toString")) none false phase))
          (when (not-in to-string-method (tag none) :narrow-true)
            (const r object (call o to-string-method (vector-of object) phase))
            (rwhen (in r primitive-object :narrow-true)
              (return r)))))
      (throw-error -type-error "cannot convert this object to a primitive"))
    
    
    (%heading (3 :semantics) "Object to Number Conversions")
    (%text :comment (:global-call object-to-general-number o phase) " returns " (:local o) " converted to a " (:global -general-number) ". If "
           (:local phase) " is " (:tag compile) ", only constant conversions are permitted.")
    (define (object-to-general-number (o object) (phase phase)) general-number
      (const a primitive-object (object-to-primitive o hint-number phase))
      (case a
        (:select undefined (return nan64))
        (:select (union null (tag false)) (return +zero64))
        (:select (tag true) (return 1.0))
        (:narrow general-number (return a))
        (:narrow (union char16 string) (return (string-to-float64 (to-string a))))))
    
    
    (%text :comment (:global-call object-to-float32 o phase) " returns " (:local o) " converted to a " (:type float32) ". If "
           (:local phase) " is " (:tag compile) ", only constant conversions are permitted.")
    (define (object-to-float32 (o object) (phase phase)) float32
      (const a primitive-object (object-to-primitive o hint-number phase))
      (case a
        (:select undefined (return nan32))
        (:select (union null (tag false)) (return +zero32))
        (:select (tag true) (return (float32 1.0)))
        (:narrow general-number (return (to-float32 a)))
        (:narrow (union char16 string) (return (string-to-float32 (to-string a))))))
    
    
    (%text :comment (:global-call object-to-float64 o phase) " returns " (:local o) " converted to a " (:type float64) ". If "
           (:local phase) " is " (:tag compile) ", only constant conversions are permitted.")
    (define (object-to-float64 (o object) (phase phase)) float64
      (return (to-float64 (object-to-general-number o phase))))
    

    (%text :comment (:global-call object-to-extended-integer o phase) " returns " (:local o) " converted to an " (:type extended-integer) ". An error occurs if "
           (:local o) " has a fractional part or is a NaN. If " (:local o) " is a string, then it is converted exactly. If "
           (:local phase) " is " (:tag compile) ", only constant conversions are permitted.")
    (define (object-to-extended-integer (o object) (phase phase)) extended-integer
      (const a primitive-object (object-to-primitive o hint-number phase))
      (case a
        (:select (union null (tag false)) (return 0))
        (:select (tag true) (return 1))
        (:select (tag undefined nan32 nan64) (return nan))
        (:select (tag +infinity32 +infinity64) (return +infinity))
        (:select (tag -infinity32 -infinity64) (return -infinity))
        (:select (tag +zero32 +zero64 -zero32 -zero64) (return 0))
        (:narrow (union long u-long) (return (& value a)))
        (:narrow (union nonzero-finite-float32 nonzero-finite-float64)
          (const r rational (& value a))
          (rwhen (not-in r integer :narrow-false)
            (throw-error -range-error "the value " (:local a) " is not an integer"))
          (return r))
        (:narrow (union char16 string)
          (return (string-to-extended-integer (to-string a))))))
    

    (%text :comment (:global-call object-to-integer o phase) " returns " (:local o) " converted to an " (:type integer) ". An error occurs if "
           (:local o) " has a fractional part or is not finite. If " (:local o) " is a string, then it is converted exactly. If "
           (:local phase) " is " (:tag compile) ", only constant conversions are permitted.")
    (define (object-to-integer (o object) (phase phase)) integer
      (const i extended-integer (object-to-extended-integer o phase))
      (case i
        (:select (tag +infinity -infinity nan) (throw-error -range-error (:local i) " is not an integer"))
        (:narrow integer (return i))))
    

    (define (string-to-float32 (s string)) float32
      (/* "Apply the lexer grammar with the start symbol " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") " to the string " (:local s) ".")
      (const q (union extended-rational (tag syntax-error))
        (lisp-call string-to-extended-rational (s)
                   (union extended-rational (tag syntax-error))
          "the result of lexing " (:operand 0) " using " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") " as the start symbol"))
      (*/)
      (cond
       ((/*/ (in q (tag syntax-error) :narrow-false)
          "the grammar cannot interpret the entire string as an expansion of " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html"))
        (return nan32))
       (nil
        (// (:local q) ":" :nbsp (:type extended-rational) " " :assign-10 " the value of the action " (:action lex) " applied to the obtained expansion of the nonterminal "
            (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") ";")
        (return (extended-rational-to-float32 q)))))
    
    
    (define (string-to-float64 (s string)) float64
      (/* "Apply the lexer grammar with the start symbol " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") " to the string " (:local s) ".")
      (const q (union extended-rational (tag syntax-error))
        (lisp-call string-to-extended-rational (s)
                   (union extended-rational (tag syntax-error))
          "the result of lexing " (:operand 0) " using " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") " as the start symbol"))
      (*/)
      (cond
       ((/*/ (in q (tag syntax-error) :narrow-false)
          "the grammar cannot interpret the entire string as an expansion of " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html"))
        (return nan64))
       (nil
        (// (:local q) ":" :nbsp (:type extended-rational) " " :assign-10 " the value of the action " (:action lex) " applied to the obtained expansion of the nonterminal "
            (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") ";")
        (return (extended-rational-to-float64 q)))))
    
    
    (define (string-to-extended-integer (s string)) extended-integer
      (/* "Apply the lexer grammar with the start symbol " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") " to the string " (:local s) ".")
      (const q (union extended-rational (tag syntax-error))
        (lisp-call string-to-extended-rational (s)
                   (union extended-rational (tag syntax-error))
          "the result of lexing " (:operand 0) " using " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") " as the start symbol"))
      (*/)
      (cond
       ((/*/ (in q (tag syntax-error) :narrow-false)
          "the grammar cannot interpret the entire string as an expansion of " (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html"))
        (throw-error -type-error "the string " (:local s) " does not contain a number"))
       (nil
        (// (:local q) ":" :nbsp (:type extended-rational) " " :assign-10 " the value of the action " (:action lex) " applied to the obtained expansion of the nonterminal "
            (:grammar-symbol :string-numeric-literal nil "lexer-semantics.html") ";")
        (case q
          (:select (tag +zero -zero) (return 0))
          (:narrow (tag +infinity -infinity nan) (return q))
          (:narrow rational
            (if (in q integer :narrow-true)
              (return q)
              (throw-error -range-error "the value should be an integer")))))))
    
    
    (%heading (3 :semantics) "Object to String Conversions")
    (%text :comment (:global-call object-to-string o phase) " returns " (:local o) " converted to a " (:global -string) ". If "
           (:local phase) " is " (:tag compile) ", only constant conversions are permitted.")
    (define (object-to-string (o object) (phase phase)) string
      (const a primitive-object (object-to-primitive o hint-string phase))
      (case a
        (:select undefined (return "undefined"))
        (:select null (return "null"))
        (:select (tag false) (return "false"))
        (:select (tag true) (return "true"))
        (:narrow general-number (return (general-number-to-string a)))
        (:narrow char16 (return (vector a)))
        (:narrow string (return a))))
    
    
    (define (to-string (o (union char16 string))) string
      (case o
        (:narrow char16 (return (vector o)))
        (:narrow string (return o))))
    
    
    (define (general-number-to-string (x general-number)) string
      (case x
        (:narrow (union long u-long) (return (integer-to-string (& value x))))
        (:narrow float32 (return (float32-to-string x)))
        (:narrow float64 (return (float64-to-string x)))))
    
    
    (%text :comment (:global-call integer-to-string i) " converts an integer " (:local i) " to a string of one or more decimal digits. If "
           (:local i) " is negative, the string is preceded by a minus sign.")
    (define (integer-to-string (i integer)) string
      (rwhen (< i 0)
        (return (cons #\- (integer-to-string (neg i)))))
      (const q integer (floor (rat/ i 10)))
      (const r integer (- i (* q 10)))
      (const c char16 (integer-to-char16 (+ r (char16-to-integer #\0))))
      (if (= q 0)
        (return (vector c))
        (return (append (integer-to-string q) (vector c)))))
    
    
    (%text :comment (:global-call integer-to-string-with-sign i) " is the same as " (:global-call integer-to-string i)
           " except that the resulting string always begins with a plus or minus sign.")
    (define (integer-to-string-with-sign (i integer)) string
      (if (>= i 0)
        (return (cons #\+ (integer-to-string i)))
        (return (cons #\- (integer-to-string (neg i))))))
    
    
    (define (exponential-notation-string (digits string) (e integer)) string
      (var mantissa string)
      (if (= (length digits) 1)
        (<- mantissa digits)
        (<- mantissa (append (vector (nth digits 0)) "." (subseq digits 1))))
      (return (append mantissa "e" (integer-to-string-with-sign e))))
    
    
    (%text :comment (:global-call float32-to-string x) " converts a " (:type float32) " " (:local x) " to a string using fixed-point notation if "
           "the absolute value of " (:local x) " is between " (:expr rational (expt 10 -6)) " inclusive and " (:expr rational (expt 10 21)) " exclusive, and "
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
            (/* (:def-const e integer) (:def-const k integer) (:def-const s integer)
                "Let " (:local e) ", " (:local k) ", and " (:local s) " be integers such that "
                (:expr boolean (>= k 1)) ", " (:expr boolean (cascade rational (expt 10 (- k 1)) <= s <= (expt 10 k))) ", "
                (:expr boolean (= (real-to-float32 (rat* s (expt 10 (- (+ e 1) k)))) x float32)) ", and " (:local k) " is as small as possible.")
            (multiple-value-bind ((digits string) (e integer)) decompose-positive-float32 (x))
            (const k integer (length digits))
            (*/)
            (note (:local k) " is the number of digits in the decimal representation of " (:local s) ", " (:local s)
                  " is not divisible by 10, and the least significant digit of " (:local s)
                  " is not necessarily uniquely determined by the above criteria.")
            (// (:def-const s integer) "When there are multiple possibilities for " (:local s) " according to the rules above, "
                "implementations are encouraged but not required to select the one according to the following rule: "
                "Select the value of " (:local s) " for which " (:expr rational (rat* s (expt 10 (- (+ e 1) k)))) " is closest in value to " (:local r)
                "; if there are two such possible values of " (:local s) ", choose the one that is even.")
            (// (:def-const s integer) (:local digits) ":" :nbsp (:type string) :nbsp :assign-10 :nbsp (:expr string (integer-to-string s)))
            (cond
             ((cascade integer (- k 1) <= e <= 20)
              (return (append digits (repeat char16 #\0 (- (+ e 1) k)))))
             ((cascade integer 0 <= e <= 20)
              (return (append (subseq digits 0 e) "." (subseq digits (+ e 1)))))
             ((cascade integer -6 <= e < 0)
              (return (append "0." (repeat char16 #\0 (neg (+ e 1))) digits)))
             (nil
              (return (exponential-notation-string digits e)))))))))
    ;(defprimitive float32-to-string (lambda (x) (float32-to-string x)))
    
    
    (%text :comment (:global-call float64-to-string x) " converts a " (:type float64) " " (:local x) " to a string using fixed-point notation if "
           "the absolute value of " (:local x) " is between " (:expr rational (expt 10 -6)) " inclusive and " (:expr rational (expt 10 21)) " exclusive, and "
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
            (/* (:def-const e integer) (:def-const k integer) (:def-const s integer)
                "Let " (:local e) ", " (:local k) ", and " (:local s) " be integers such that "
                (:expr boolean (>= k 1)) ", " (:expr boolean (cascade rational (expt 10 (- k 1)) <= s <= (expt 10 k))) ", "
                (:expr boolean (= (real-to-float64 (rat* s (expt 10 (- (+ e 1) k)))) x float64)) ", and " (:local k) " is as small as possible.")
            (multiple-value-bind ((digits string) (e integer)) decompose-positive-float64 (x))
            (const k integer (length digits))
            (*/)
            (note (:local k) " is the number of digits in the decimal representation of " (:local s) ", " (:local s)
                  " is not divisible by 10, and the least significant digit of " (:local s)
                  " is not necessarily uniquely determined by the above criteria.")
            (// (:def-const s integer) "When there are multiple possibilities for " (:local s) " according to the rules above, "
                "implementations are encouraged but not required to select the one according to the following rule: "
                "Select the value of " (:local s) " for which " (:expr rational (rat* s (expt 10 (- (+ e 1) k)))) " is closest in value to " (:local r)
                "; if there are two such possible values of " (:local s) ", choose the one that is even.")
            (// (:def-const s integer) (:local digits) ":" :nbsp (:type string) :nbsp :assign-10 :nbsp (:expr string (integer-to-string s)))
            (cond
             ((cascade integer (- k 1) <= e <= 20)
              (return (append digits (repeat char16 #\0 (- (+ e 1) k)))))
             ((cascade integer 0 <= e <= 20)
              (return (append (subseq digits 0 e) "." (subseq digits (+ e 1)))))
             ((cascade integer -6 <= e < 0)
              (return (append "0." (repeat char16 #\0 (neg (+ e 1))) digits)))
             (nil
              (return (exponential-notation-string digits e)))))))))
    ;(defprimitive float64-to-string (lambda (x) (float64-to-string x)))
    
    
    (%heading (3 :semantics) "Object to Qualified Name Conversion")
    (%text :comment (:global-call object-to-qualified-name o phase) " coerces an object " (:local o) " to a qualified name. If "
           (:local phase) " is " (:tag compile) ", only constant conversions are permitted.")
    (define (object-to-qualified-name (o object) (phase phase)) qualified-name
      (return (new qualified-name public (object-to-string o phase))))
    
    
    (%heading (3 :semantics) "Object to Class Conversion")
    (%text :comment (:global-call object-to-class o) " returns " (:local o) " converted to a non-" (:tag null) " " (:global -class) ".")
    (define (object-to-class (o object)) class
      (if (in o class :narrow-true)
        (return o)
        (throw-error -type-error)))
    
    
    (%heading (3 :semantics) "Object to Attribute Conversion")
    (%text :comment (:global-call object-to-attribute o) " returns " (:local o) " converted to an attribute.")
    (define (object-to-attribute (o object) (phase phase)) attribute
      (cond
       ((in o attribute :narrow-true) (return o))
       (nil
        (note "If " (:local o) " is not an attribute, try to call it with no arguments.")
        (const a object (call null o (vector-of object) phase))
        (if (in a attribute :narrow-true)
          (return a)
          (throw-error -type-error)))))
    
    
    (%heading (3 :semantics) "Implicit Coercions")
    (%text :comment (:global-call coerce o c) " attempts to implicitly coerce " (:local o) " to class " (:local c) ". If the coercion succeeds, "
           (:global coerce) " returns the coerced value. If not, " (:global coerce) " throws a " (:global -type-error) ".")
    (%text :comment "The coercion always succeeds and returns " (:local o) " unchanged if " (:local o) " is already a member of class " (:local c) ". The value returned from "
           (:global coerce) " always is a member of class " (:local c) ".")
    (define (coerce (o object) (c class)) object
      (const result object-opt ((& coerce c) o c))
      (if (not-in result (tag none) :narrow-true)
        (return result)
        (throw-error -type-error "coercion failed")))
    
    
    (%text :comment (:global-call coerce-or-null o c) " attempts to implicitly coerce " (:local o) " to class " (:local c) ". If the coercion succeeds, "
           (:global coerce-or-null) " returns the coerced value. If not, then " (:global coerce-or-null) " returns " (:tag null) " if "
           (:tag null) " is a member of type " (:local c) "; otherwise, " (:global coerce-or-null) " throws a " (:global -type-error) ".")
    (%text :comment "The coercion always succeeds and returns " (:local o) " unchanged if " (:local o) " is already a member of class " (:local c) ". The value returned from "
           (:global coerce-or-null) " always is a member of class " (:local c) ".")
    (define (coerce-or-null (o object) (c class)) object
      (const result object-opt ((& coerce c) o c))
      (cond
       ((not-in result (tag none) :narrow-true)
        (return result))
       ((= ((& coerce c) null c) null object-opt)
        (return null))
       (nil (throw-error -type-error "coercion failed"))))
    
    
    (%text :comment (:global-call coerce-non-null o c) " attempts to implicitly coerce " (:local o) " to class " (:local c)
           ". If the coercion succeeds and the result is not " (:tag null) ", then "
           (:global coerce-non-null) " returns the coerced value. If not, " (:global coerce-non-null) " throws a " (:global -type-error) ".")
    (define (coerce-non-null (o object) (c class)) object
      (const result object-opt ((& coerce c) o c))
      (if (not-in result (tag none null) :narrow-true)
        (return result)
        (throw-error -type-error "coercion failed")))
    
    
    (%text :comment (:global-call ordinary-coerce o c) " is the implementation of coercion for a native class unless specified otherwise in the class" :apostrophe
           "s definition. Host classes may define a different procedure to perform this coercion.")
    (define (ordinary-coerce (o object) (c class)) object-opt
      (if (or (in o (tag null)) (is o c))
        (return o)
        (return none)))
    
    
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
          (return (new compound-attribute (list-set a b) false false false none none false false)))
         (nil
          (return (set-field b namespaces (set+ (& namespaces b) (list-set a)))))))
       ((in b namespace :narrow-both)
        (return (set-field a namespaces (set+ (& namespaces a) (list-set b)))))
       (nil
        (note "At this point both " (:local a) " and " (:local b) " are compound attributes.")
        (if (or (and (not-in (& category a) (tag none)) (not-in (& category b) (tag none)) (/= (& category a) (& category b) property-category))
                (and (not-in (& override-mod a) (tag none)) (not-in (& override-mod b) (tag none)) (/= (& override-mod a) (& override-mod b) override-modifier)))
          (throw-error -attribute-error "attributes " (:local a) " and " (:local b) " have conflicting contents")
          (return (new compound-attribute
                       (set+ (& namespaces a) (& namespaces b))
                       (or (& explicit a) (& explicit b))
                       (or (& enumerable a) (& enumerable b))
                       (or (& dynamic a) (& dynamic b))
                       (if (not-in (& category a) (tag none)) (& category a) (& category b))
                       (if (not-in (& override-mod a) (tag none)) (& override-mod a) (& override-mod b))
                       (or (& prototype a) (& prototype b))
                       (or (& unused a) (& unused b))))))))
    
    
    (%text :comment (:global-call to-compound-attribute a) " returns " (:local a) " converted to a " (:type compound-attribute)
           " even if it was a simple namespace, " (:tag true) ", or " (:tag none) ".")
    (define (to-compound-attribute (a attribute-opt-not-false)) compound-attribute
      (case a
        (:select (tag none true) (return (new compound-attribute (list-set-of namespace) false false false none none false false)))
        (:narrow namespace (return (new compound-attribute (list-set a) false false false none none false false)))
        (:narrow compound-attribute (return a))))
    
    
    (%heading (2 :semantics) "Access Utilities")
    (%text :comment (:global-call accesses-overlap accesses1 accesses2) " returns " (:tag true) " if the two " (:type access-set) "s have a nonempty intersection.")
    (define (accesses-overlap (accesses1 access-set) (accesses2 access-set)) boolean
      (return (or (= accesses1 accesses2 access-set)
                  (in accesses1 (tag read-write))
                  (in accesses2 (tag read-write)))))
    
    
    (define (archetype (o object)) object-opt
      (case o
        (:select (union undefined null) (return none))
        (:select boolean (return (&opt prototype -boolean)))
        (:select long (return (&opt prototype \#long)))
        (:select u-long (return (&opt prototype ulong)))
        (:select float32 (return (&opt prototype float)))
        (:select float64 (return (&opt prototype -number)))
        (:select char16 (return (&opt prototype char)))
        (:select string (return (&opt prototype -string)))
        (:select namespace (return (&opt prototype -namespace)))
        (:select compound-attribute (return (&opt prototype -attribute)))
        (:select method-closure (return (&opt prototype -function)))
        (:select class (return (&opt prototype -class)))
        (:narrow (union simple-instance reg-exp date package) (return (&opt archetype o)))))
    
    
    (%text :comment (:global-call archetypes o) " returns the set of " (:local o) :apostrophe "s archetypes, not including " (:local o) " itself.")
    (define (archetypes (o object)) (list-set object)
      (const a object-opt (archetype o))
      (rwhen (in a (tag none) :narrow-false)
        (return (list-set-of object)))
      (return (set+ (list-set a) (archetypes a))))
    
    
    (%text :comment (:local o) " is an object that is known to have slot " (:local id) ". " (:global-call find-slot o id) " returns that slot.")
    (define (find-slot (o object) (id instance-variable)) slot
      (assert (in o (union simple-instance method-closure) :narrow-true)
        (:local o) " must be a " (:type simple-instance) " or a " (:type method-closure) " in order to have slots.")
      (const matching-slots (list-set slot)
        (map (& slots o) s s (= (& id s) id instance-variable)))
      (return (unique-elt-of matching-slots)))
    
    
    (%text :comment (:global-call setup-variable v) " runs " (:action setup) " and initialises the type of the variable " (:local v)
           ", making sure that " (:action setup) " is done at most once and does not reenter itself.")
    (define (setup-variable (v variable)) void
      (const setup (union (-> () class-opt) (tag none busy)) (& setup v))
      (case setup
        (:narrow (-> () class-opt)
          (&= setup v busy)
          (var type class-opt (setup))
          (when (in type (tag none))
            (<- type -object))
          (quiet-assert (not-in type (tag none) :narrow-true))
          (&const= type v type)
          (&= setup v none))
        (:select (tag none))
        (:select (tag busy)
          (throw-error -constant-error "a constant" :apostrophe "s type or initialiser cannot depend on the value of that constant"))))
    
    
    (%text :comment (:def-const v variable) (:global-call write-variable v new-value clear-initializer) " writes the value " (:local new-value) " into the mutable or immutable variable "
           (:local v) ". " (:local new-value) " is coerced to " (:local v) :apostrophe "s type. If the " (:local clear-initializer) " flag is set, then the caller "
           " has just evaluated " (:local v) :apostrophe "s initialiser and is supplying its result in " (:local new-value) ". In this case " (:global write-variable)
           " atomically clears " (:expr (union initializer (tag none busy)) (& initializer v)) " while writing "
           (:expr variable-value (& value v)) ". In all other cases the presence of an initialiser or an existing value will prevent an immutable variable" :apostrophe
           "s value from being written.")
    (define (write-variable (v variable) (new-value object) (clear-initializer boolean)) object
      (const coerced-value object (coerce new-value (&opt type v)))
      (when clear-initializer
        (&= initializer v none))
      (rwhen (and (& immutable v) (or (not-in (& value v) (tag none)) (not-in (& initializer v) (tag none))))
        (throw-error -reference-error "cannot initialise a " (:character-literal "const") " variable twice"))
      (&= value v coerced-value)
      (return coerced-value))
    
    
    (%heading (2 :semantics) "Environmental Utilities")
    
    (%text :comment "If " (:local env) " is from within a class" :apostrophe "s body, "
           (:global-call get-enclosing-class env) " returns the innermost such class; otherwise, it returns " (:tag none) ".")
    (define (get-enclosing-class (env environment)) class-opt
      (reserve c)
      (rwhen (some env c (in c class :narrow-true) :define-true)
        (// "Let " (:local c) " be the first element of " (:local env) " that is a " (:type class) ".")
        (return c))
      (return none))
    
    
    (%text :comment "If " (:local env) " is from within a function" :apostrophe "s body, "
           (:global-call get-enclosing-parameter-frame env) " returns the " (:type parameter-frame) " for the innermost such function; otherwise, it returns " (:tag none) ".")
    (define (get-enclosing-parameter-frame (env environment)) parameter-frame-opt
      (for-each env frame
        (case frame
          (:select (union local-frame with-frame))
          (:narrow parameter-frame (return frame))
          (:select (union package class) (return none))))
      (return none))
    
    
    (%text :comment (:global-call get-regional-environment env) " returns all frames in " (:local env) " up to and including the first regional frame. "
           "A regional frame is either any frame other than a with frame or local block frame, a local block frame directly enclosed in a class, or "
           "a local block frame directly enclosed in a with frame directly enclosed in a class.")
    (define (get-regional-environment (env environment)) (vector frame)
      (var i integer 0)
      (while (in (nth env i) (union local-frame with-frame))
        (<- i (+ i 1)))
      (when (in (nth env i) class)
        (while (and (/= i 0) (not-in (nth env i) local-frame))
          (<- i (- i 1))))
      (return (subseq env 0 i)))
    
    
    (%text :comment (:global-call get-regional-frame env) " returns the most specific regional frame in " (:local env) ".")
    (define (get-regional-frame (env environment)) frame
      (const regional-env (vector frame) (get-regional-environment env))
      (return (nth regional-env (- (length regional-env) 1))))
    
    
    (%text :comment (:global-call get-package-frame env) " returns the innermost package frame in " (:local env) ".")
    (define (get-package-frame (env environment)) package
      (var i integer 0)
      (while (not-in (nth env i) package)
        (<- i (+ i 1)))
      (note "Every environment ends with a " (:type package) " frame, so one will always be found.")
      (return (assert-in (nth env i) package)))
    
    
    
    (%heading (2 :semantics) "Property Lookup")
    
    (%text :comment (:global-call find-local-singleton-property o multiname access) " looks in " (:local o) " for a local singleton property with one of the names in "
           (:local multiname) " and access that includes " (:local access) ". If there is no such property, " (:global find-local-singleton-property)
           " returns " (:tag none) ". If there is exactly one such property, " (:global find-local-singleton-property)
           " returns it. If there is more than one such property, " (:global find-local-singleton-property) " throws an error.")
    (define (find-local-singleton-property (o (union non-with-frame simple-instance reg-exp date)) (multiname multiname) (access access))
            singleton-property-opt
      (const matching-local-bindings (list-set local-binding)
        (map (& local-bindings o) b b (and (set-in (& qname b) multiname) (accesses-overlap (& accesses b) access))))
      (note "If the same property was found via several different bindings " (:local b)
            ", then it will appear only once in the set " (:local matching-properties) ".")
      (const matching-properties (list-set singleton-property) (map matching-local-bindings b (& content b)))
      (cond
       ((empty matching-properties)
        (return none))
       ((= (length matching-properties) 1)
        (return (unique-elt-of matching-properties)))
       (nil
        (throw-error -reference-error "this access is ambiguous because the bindings it found belong to several different local properties"))))
    
    
    (%text :comment (:global-call instance-property-accesses m) " returns instance property" :apostrophe "s " (:type access-set) ".")
    (define (instance-property-accesses (m instance-property)) access-set
      (case m
        (:select (union instance-variable instance-method) (return read-write))
        (:select instance-getter (return read))
        (:select instance-setter (return write))))
    
    
    (%text :comment (:global-call find-local-instance-property c multiname accesses) " looks in class " (:local c) " for a local instance property with one of the names in "
           (:local multiname) " and accesses that have a nonempty intersection with " (:local accesses) ". If there is no such property, " (:global find-local-instance-property)
           " returns " (:tag none) ". If there is exactly one such property, " (:global find-local-instance-property)
           " returns it. If there is more than one such property, " (:global find-local-instance-property) " throws an error.")
    (define (find-local-instance-property (c class) (multiname multiname) (accesses access-set))
            instance-property-opt
      (const matches (list-set instance-property)
        (map (& instance-properties c) m m (and (nonempty (set* (&opt multiname m) multiname)) (accesses-overlap (instance-property-accesses m) accesses))))
      (cond
       ((empty matches)
        (return none))
       ((= (length matches) 1)
        (return (unique-elt-of matches)))
       (nil
        (throw-error -reference-error "this access is ambiguous because it found several different instance properties in the same class"))))
    
    
    (%text :comment (:global-call find-archetype-property o multiname access flat) " looks in object " (:local o)
           " for any local or inherited property with one of the names in "
           (:local multiname) " and access that includes " (:local access) ". If " (:local flat) " is " (:tag true)
           ", then properties inherited from the archetype are not considered in the search. If it finds no property, " (:global find-archetype-property)
           " returns " (:tag none) ". If it finds one property, " (:global find-archetype-property)
           " returns it. If it finds more than one property, " (:global find-archetype-property) " prefers the more local one in the list of " (:local o) :apostrophe
           "s superclasses or archetypes; if two or more properties remain, the singleton one is preferred; if two or more properties still remain, "
           (:global find-archetype-property) " throws an error.")
    (%text :comment "Note that " (:global-call find-archetype-property o multiname access flat) " searches " (:local o) " itself rather than " (:local o) :apostrophe
           "s class for properties. " (:global find-archetype-property) " will not find instance properties unless " (:local o) " is a class.")
    (define (find-archetype-property (o object) (multiname multiname) (access access) (flat boolean))
            property-opt
      (var m property-opt)
      (case o
        (:select (union undefined null boolean long u-long float32 float64 char16 string namespace compound-attribute method-closure)
          (<- m none))
        (:narrow (union simple-instance reg-exp date package)
          (<- m (find-local-singleton-property o multiname access)))
        (:narrow class
          (<- m (find-class-property o multiname access))))
      (rwhen (not-in m (tag none))
        (return m))
      (rwhen flat
        (return none))
      (const a object-opt (archetype o))
      (rwhen (in a (tag none) :narrow-false)
        (return none))
      (return (find-archetype-property a multiname access flat)))
    
    
    (define (find-class-property (c class) (multiname multiname) (access access))
            property-opt
      (var m property-opt (find-local-singleton-property c multiname access))
      (when (in m (tag none))
        (<- m (find-local-instance-property c multiname access))
        (when (in m (tag none))
          (const super class-opt (& super c))
          (when (not-in super (tag none) :narrow-true)
            (<- m (find-class-property super multiname access)))))
      (return m))
    
    
    (%text :comment (:global-call find-base-instance-property c multiname accesses) " looks in class " (:local c)
           " and its ancestors for an instance property with one of the names in "
           (:local multiname) " and accesses that have a nonempty intersection with " (:local accesses) ". If there is no such property, " (:global find-base-instance-property)
           " returns " (:tag none) ". If there is exactly one such property, " (:global find-base-instance-property)
           " returns it. If there is more than one such property, " (:global find-base-instance-property)
           " prefers the one defined in the least specific class; if two or more properties still remain, "
           (:global find-base-instance-property) " throws an error.")
    (define (find-base-instance-property (c class) (multiname multiname) (accesses access-set))
            instance-property-opt
      (note "Start from the root class (" (:character-literal "Object") ") and proceed through more specific classes that are ancestors of " (:local c) ".")
      (for-each (ancestors c) s
        (const m instance-property-opt (find-local-instance-property s multiname accesses))
        (rwhen (not-in m (tag none))
          (return m)))
      (return none))
    
    
    (%text :comment (:global-call get-derived-instance-property c m-base accesses) " returns the most derived instance property whose name includes that of " (:local m-base)
           " and whose accesses that have a nonempty intersection with " (:local accesses) ". The caller of " (:global get-derived-instance-property)
           " ensures that such an instance property always exists. "
           "If " (:local accesses) " is " (:tag read-write) " then it is possible that this search could find both a getter and a setter defined in the same class; "
           "in this case either the getter or the setter is returned at the implementation" :apostrophe "s discretion.")
    (define (get-derived-instance-property (c class) (m-base instance-property) (accesses access-set)) instance-property
      (reserve m)
      (if (some (& instance-properties c) m (and (set<= (&opt multiname m-base) (&opt multiname m) multiname)
                                                 (accesses-overlap (instance-property-accesses m) accesses)) :define-true)
        (return m)
        (return (get-derived-instance-property (assert-not-in (& super c) (tag none)) m-base accesses))))
    
     
    (%text :comment (:global-call read-implicit-this env) " returns the value of implicit " (:character-literal "this")
           " to be used to access instance properties within a class" :apostrophe "s scope without using the " (:character-literal ".") " operator. An implicit "
           (:character-literal "this") " is well-defined only inside instance methods and constructors; " (:global read-implicit-this)
           " throws an error if there is no well-defined implicit " (:character-literal "this") " value or if an attempt is made to read it before it has been initialised.")
    (define (read-implicit-this (env environment)) object
      (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
      (rwhen (in frame (tag none) :narrow-false)
        (throw-error -reference-error "can" :apostrophe "t access instance properties outside an instance method without supplying an instance object"))
      (const this object-opt (& this frame))
      (rwhen (in this (tag none) :narrow-false)
        (throw-error -reference-error "can" :apostrophe "t access instance properties inside a non-instance method without supplying an instance object"))
      (rwhen (not-in (& kind frame) (tag instance-function constructor-function))
        (throw-error -reference-error "can" :apostrophe "t access instance properties inside a non-instance method without supplying an instance object"))
      (rwhen (not (& superconstructor-called frame))
        (throw-error -uninitialized-error "can" :apostrophe "t access instance properties from within a constructor before the superconstructor has been called"))
      (return this))
    
    
    (%text :comment (:global-call has-property o property flat phase) " returns " (:tag true) " if " (:local o) " has a readable or writable property named "
           (:local property) ". If " (:local flat) " is " (:tag true) ", then properties inherited from the archetype are not considered.")
    (define (has-property (o object) (property object) (flat boolean) (phase phase)) boolean
      (const c class (object-type o))
      (return ((& has-property c) o c property flat phase)))
    
    
    (%text :comment (:global-call has-property o c property flat phase) " is the implementation of " (:global has-property)
           " for a native class unless specified otherwise in the class" :apostrophe
           "s definition. Host classes may either also use " (:global ordinary-has-property) " or define a different procedure to perform this test. "
           (:local c) " is " (:local o) :apostrophe "s type.")
    (define (ordinary-has-property (o object) (c class) (property object) (flat boolean) (phase phase)) boolean
      (const qname qualified-name (object-to-qualified-name property phase))
      (return (or (not-in (find-base-instance-property c (list-set qname) read) (tag none))
                  (not-in (find-base-instance-property c (list-set qname) write) (tag none))
                  (not-in (find-archetype-property o (list-set qname) read flat) (tag none))
                  (not-in (find-archetype-property o (list-set qname) write flat) (tag none)))))
    
    
    (%heading (2 :semantics) "Reading")
    (%text :comment "If " (:local r) " is an " (:type object) ", " (:global-call read-reference r phase) " returns it unchanged.  If "
           (:local r) " is a " (:type reference) ", " (:global read-reference) " reads " (:local r) " and returns the result. If "
           (:local phase) " is " (:tag compile) ", only constant expressions can be evaluated in the process of reading " (:local r) ".")
    (define (read-reference (r obj-or-ref) (phase phase)) object
      (var result object-opt)
      (case r
        (:narrow object (<- result r))
        (:narrow lexical-reference (<- result (lexical-read (& env r) (& variable-multiname r) phase)))
        (:narrow dot-reference
          (<- result ((& read (& limit r)) (& base r) (& limit r) (& multiname r) none true phase)))
        (:narrow bracket-reference
          (<- result ((& bracket-read (& limit r)) (& base r) (& limit r) (& args r) true phase))))
      (if (not-in result (tag none) :narrow-true)
        (return result)
        (throw-error -reference-error "property not found, and no default value is available")))
    
    
    (%text :comment (:global-call dot-read o multiname phase) " reads and returns the value of the " (:local multiname) " property of " (:local o) ". "
           (:global dot-read) " throws an error if the property does not exist and no default value was available for it.")
    (define (dot-read (o object) (multiname multiname) (phase phase))
            object
      (const limit class (object-type o))
      (const result object-opt ((& read limit) o limit multiname none true phase))
      (rwhen (in result (tag none) :narrow-false)
        (throw-error -reference-error "property not found, and no default value is available"))
      (return result))
    
    
    (%text :comment (:global-call read-length o phase) " reads and returns the value of the " (:character-literal "length") " property of " (:local o)
           ", ensuring that it is an integer between 0 and " (:global array-limit) " inclusive.")
    (define (read-length (o object) (phase phase)) integer
      (var value object (dot-read o (list-set (new qualified-name public "length")) phase))
      (rwhen (not-in value general-number :narrow-false)
        (throw-error -type-error "length not an integer"))
      (const length integer-opt (check-integer value))
      (cond
       ((in length (tag none) :narrow-false) (throw-error -range-error "length not an integer"))
       ((cascade integer 0 <= length <= array-limit) (return length))
       (nil (throw-error -range-error "length out of range"))))
    
    
    (%text :comment (:global-call index-read o i phase) " returns the value of " (:local o) (:character-literal "[") (:local i) (:character-literal "]")
           " or " (:tag none) " if no such property was found; unlike " (:global dot-read) ", " (:global index-read)
           " does not return a default value for missing properties. " (:local i) " should always be a valid array index.")
    (define (index-read (o object) (i integer) (phase phase))
            object-opt
      (assert (cascade integer 0 <= i < array-limit))
      (const limit class (object-type o))
      (const x float64 (real-to-float64 i))
      (const result object-opt ((& bracket-read limit) o limit (vector x) false phase))
      (when (and (not-in result (tag none)) (not (has-property o x true phase)))
        (// "At the implementation" :apostrophe "s discretion either do nothing, set " (:local result) " to " (:tag none) ", or "
            (:keyword throw) " a " (:global -reference-error) "."))
      (return result))
    
    
    (%text :comment (:def-const o object) (:global-call ordinary-bracket-read o limit args undefined-if-missing phase) " evaluates the expression "
           (:local o) (:character-literal "[") (:local args) (:character-literal "]") " when " (:local o) " is a native object. Host objects may either also use "
           (:global ordinary-bracket-read) " or choose a different procedure " (:local -p) " to evaluate " (:local o) (:character-literal "[") (:local args) (:character-literal "]")
           " by writing " (:local -p) " into " (:expr (-> (object class (vector object) boolean phase) object-opt) (& bracket-read (object-type o))) ".")
    (%text :comment (:def-const o object) (:local limit) " is used to handle the expression "
           (:character-literal "super(") (:local o) (:character-literal ")[") (:local args) (:character-literal "]")
           ", in which case " (:local limit) " is the superclass of the class inside which the " (:character-literal "super") " expression appears. "
           "Otherwise, " (:local limit) " is set to " (:expr class (object-type o)) ".")
    (define (ordinary-bracket-read (o object) (limit class) (args (vector object)) (undefined-if-missing boolean) (phase phase)) object-opt
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const qname qualified-name (object-to-qualified-name (nth args 0) phase))
      (return ((& read limit) o limit (list-set qname) none undefined-if-missing phase)))
    
    
    (define (lexical-read (env environment) (multiname multiname) (phase phase)) object
      (var i integer 0)
      (while (< i (length env))
        (const frame frame (nth env i))
        (var result object-opt none)
        (case frame
          (:narrow (union package class)
            (const limit class (object-type frame))
            (<- result ((& read limit) frame limit multiname env false phase)))
          (:narrow (union parameter-frame local-frame)
            (const m singleton-property-opt (find-local-singleton-property frame multiname read))
            (when (not-in m (tag none) :narrow-true)
              (<- result (read-singleton-property m phase))))
          (:narrow with-frame
            (const value object-opt (& value frame))
            (rwhen (in value (tag none) :narrow-false)
              (case phase
                (:select (tag compile)
                  (throw-error -constant-error "cannot read a " (:character-literal "with") " statement" :apostrophe "s frame from a constant expression"))
                (:select (tag run)
                  (throw-error -uninitialized-error
                    "cannot read a " (:character-literal "with") " statement" :apostrophe "s frame before that statement" :apostrophe
                    "s expression has been evaluated"))))
            (const limit class (object-type value))
            (<- result ((& read limit) value limit multiname env false phase))))
        (rwhen (not-in result (tag none) :narrow-true)
          (return result))
        (<- i (+ i 1)))
      (throw-error -reference-error "no property found with the name " (:local multiname)))
    
    
    (define (ordinary-read (o object) (limit class) (multiname multiname) (env environment-opt) (undefined-if-missing boolean) (phase phase))
            object-opt
      (const m-base instance-property-opt (find-base-instance-property limit multiname read))
      (rwhen (not-in m-base (tag none) :narrow-true)
        (return (read-instance-property o limit m-base phase)))
      (rwhen (/= limit (object-type o) class)
        (return none))
      (const flat boolean (and (not-in env (tag none)) (in o class)))
      (const m property-opt (find-archetype-property o multiname read flat))
      (case m
        (:select (tag none)
          (if (and undefined-if-missing
                   (in o (union simple-instance date reg-exp package) :narrow-true)
                   (not (& sealed o)))
            (case phase
              (:select (tag compile) (throw-error -constant-error "a constant expression cannot read dynamic properties"))
              (:select (tag run) (return undefined)))
            (return none)))
        (:narrow singleton-property (return (read-singleton-property m phase)))
        (:narrow instance-property
          (rwhen (or (not-in o class :narrow-false) (in env (tag none) :narrow-false))
            (throw-error -reference-error "cannot read an instance property without supplying an instance"))
          (const this object (read-implicit-this env))
          (return (read-instance-property this (object-type this) m phase)))))
    
    
    (%text :comment (:global-call read-instance-property o qname phase) " is a simplified interface to "
           (:global ordinary-read) " used to read instance slots that are known to exist.")
    (define (read-instance-slot (o object) (qname qualified-name) (phase phase))
            object
      (const c class (object-type o))
      (const m-base instance-property-opt (find-base-instance-property c (list-set qname) read))
      (assert (not-in m-base (tag none) :narrow-true)
        (:global read-instance-property) " is only called in cases where the instance property is known to exist, so " (:local m-base) " cannot be " (:tag none) " here.")
      (return (read-instance-property o c m-base phase)))
    
    
    (define (read-instance-property (this object) (c class) (m-base instance-property) (phase phase))
            object
      (const m instance-property (get-derived-instance-property c m-base read))
      (case m
        (:narrow instance-variable
          (rwhen (and (in phase (tag compile)) (not (& immutable m)))
            (throw-error -constant-error "a constant expression cannot read mutable variables"))
          (const v object-opt (& value (find-slot this m)))
          (rwhen (in v (tag none) :narrow-false)
            (case phase
              (:select (tag compile) (throw-error -constant-error "cannot read uninitalised " (:character-literal "const") " variables from a constant expression"))
              (:select (tag run) (throw-error -uninitialized-error "cannot read a " (:character-literal "const") " instance variable before it is initialised"))))
          (return v))
        (:narrow instance-method
          (const slots (list-set slot) (list-set (new slot ivar-function-length (real-to-float64 (& length m)))))
          (return (new method-closure this m slots)))
        (:narrow instance-getter
          (return ((& call m) this phase)))
        (:narrow instance-setter
          (bottom (:local m) " cannot be an " (:type instance-setter) " because these are only represented as write-only properties."))))
    
    
    (define (read-singleton-property (m singleton-property) (phase phase)) object
      (case m
        (:select (tag forbidden)
          (throw-error -reference-error "cannot access a property defined in a scope outside the current region if any block inside the current region shadows it"))
        (:narrow dynamic-var
          (rwhen (in phase (tag compile))
            (throw-error -constant-error "a constant expression cannot read mutable variables"))
          (var value (union object uninstantiated-function) (& value m))
          (assert (not-in value uninstantiated-function :narrow-true)
            (:local value) " can be an " (:type uninstantiated-function) " only during the " (:tag compile) " phase, which was ruled out above.")
          (return value))
        (:narrow variable
          (rwhen (and (in phase (tag compile)) (not (& immutable m)))
            (throw-error -constant-error "a constant expression cannot read mutable variables"))
          (const value variable-value (& value m))
          (case value
            (:narrow object (return value))
            (:select (tag none)
              (rwhen (not (& immutable m))
                (throw-error -uninitialized-error))
              (note "Try to run a " (:character-literal "const") " variable" :apostrophe "s initialiser if there is one.")
              (setup-variable m)
              (const initializer (union initializer (tag none busy)) (& initializer m))
              (rwhen (in initializer (tag none busy) :narrow-false)
                (case phase
                  (:select (tag compile) (throw-error -constant-error "a constant expression cannot access a constant with a missing or recursive initialiser"))
                  (:select (tag run) (throw-error -uninitialized-error))))
              (&= initializer m busy)
              (var coerced-value object)
              (catch ((const new-value object (initializer (&opt initializer-env m) compile))
                      (<- coerced-value (write-variable m new-value true)))
                (x)
                (note "If initialisation failed, restore " (:expr (union initializer (tag none busy)) (& initializer m)) " to its original value so it can be tried later.")
                (&= initializer m initializer)
                (throw x))
              (return coerced-value))
            (:select uninstantiated-function
              (assert (in phase (tag compile)) "An uninstantiated function can only be found when " (:assertion) ".")
              (throw-error -constant-error "an uninstantiated function is not a constant expression"))))
        (:narrow getter
          (const env environment-opt (& env m))
          (rwhen (in env (tag none) :narrow-false)
            (assert (in phase (tag compile)) "An uninstantiated getter can only be found when " (:assertion) ".")
            (throw-error -constant-error "an uninstantiated getter is not a constant expression"))
          (return ((& call m) env phase)))
        (:narrow setter
          (bottom (:local m) " cannot be a " (:type setter) " because these are only represented as write-only properties."))))
    
    
    (%heading (2 :semantics) "Writing")
    (%text :comment "If " (:local r) " is a reference, " (:global-call write-reference r new-value) " writes " (:local new-value) 
           " into " (:local r) ". An error occurs if " (:local r) " is not a reference. "
           (:global write-reference) " is never called from a constant expression.")
    (define (write-reference (r obj-or-ref) (new-value object) (phase (tag run))) void
      (var result (tag none ok))
      (case r
        (:select object (throw-error -reference-error "a non-reference is not a valid target of an assignment"))
        (:narrow lexical-reference
          (lexical-write (& env r) (& variable-multiname r) new-value (not (& strict r)) phase)
          (<- result ok))
        (:narrow dot-reference
          (<- result ((& write (& limit r)) (& base r) (& limit r) (& multiname r) none new-value true phase)))
        (:narrow bracket-reference
          (<- result ((& bracket-write (& limit r)) (& base r) (& limit r) (& args r) new-value true phase))))
      (rwhen (in result (tag none))
        (throw-error -reference-error "property not found and could not be created")))
    
    
    (%text :comment (:global-call dot-write o multiname new-value phase) " is a simplified interface to write " (:local new-value)
           " into the " (:local multiname) " property of " (:local o) ".")
    (define (dot-write (o object) (multiname multiname) (new-value object) (phase (tag run)))
            void
      (const limit class (object-type o))
      (const result (tag none ok) ((& write limit) o limit multiname none new-value true phase))
      (rwhen (in result (tag none))
        (throw-error -reference-error "property not found and could not be created")))
    
    
    (%text :comment (:global-call write-length o length phase) " ensures that " (:local length) " is between 0 and " (:global array-limit)
           " inclusive and then writes it into the " (:character-literal "length") " property of " (:local o) ". Note that if " (:local o)
           " is an " (:character-literal "Array") ", the act of writing its " (:character-literal "length") " property will invoke the "
           (:global -array_set-length) " setter.")
    (define (write-length (o object) (length integer) (phase (tag run))) void
      (rwhen (or (< length 0) (> length array-limit))
        (throw-error -range-error "length out of range"))
      (dot-write o (list-set (new qualified-name public "length")) (real-to-float64 length) phase))
    
    
    (define (index-write (o object) (i integer) (new-value object-opt) (phase (tag run)))
            void
      (rwhen (or (< i 0) (>= i array-limit))
        (throw-error -range-error "index out of range"))
      (const limit class (object-type o))
      (cond
       ((in new-value (tag none) :narrow-false)
        (const delete-result boolean-opt ((& bracket-delete limit) o limit (vector (real-to-float64 i)) phase))
        (rwhen (in delete-result (tag false))
          (throw-error -reference-error "cannot delete element")))
       (nil
        (const write-result (tag none ok) ((& bracket-write limit) o limit (vector (real-to-float64 i)) new-value true phase))
        (rwhen (in write-result (tag none))
          (throw-error -reference-error "element not found and could not be created")))))
    
    
    (define (ordinary-bracket-write (o object) (limit class) (args (vector object)) (new-value object) (create-if-missing boolean) (phase (tag run)))
            (tag none ok)
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const qname qualified-name (object-to-qualified-name (nth args 0) phase))
      (return ((& write limit) o limit (list-set qname) none new-value create-if-missing phase)))
    
    
    (define (lexical-write (env environment) (multiname multiname) (new-value object) (create-if-missing boolean) (phase (tag run))) void
      (var i integer 0)
      (while (< i (length env))
        (const frame frame (nth env i))
        (var result (tag none ok) none)
        (case frame
          (:narrow (union package class)
            (const limit class (object-type frame))
            (<- result ((& write limit) frame limit multiname env new-value false phase)))
          (:narrow (union parameter-frame local-frame)
            (const m singleton-property-opt (find-local-singleton-property frame multiname write))
            (when (not-in m (tag none) :narrow-true)
              (write-singleton-property m new-value phase)
              (<- result ok)))
          (:narrow with-frame
            (const value object-opt (& value frame))
            (rwhen (in value (tag none) :narrow-false)
              (throw-error -uninitialized-error
                "cannot read a " (:character-literal "with") " statement" :apostrophe "s frame before that statement" :apostrophe
                "s expression has been evaluated"))
            (const limit class (object-type value))
            (<- result ((& write limit) value limit multiname env new-value false phase))))
        (rwhen (in result (tag ok))
          (return))
        (<- i (+ i 1)))
      (when create-if-missing
        (const pkg package (get-package-frame env))
        (note "Try to write the variable into " (:local pkg) " again, this time allowing new dynamic bindings to be created dynamically.")
        (const limit class (object-type pkg))
        (const result (tag none ok) ((& write limit) pkg limit multiname env new-value true phase))
        (rwhen (in result (tag ok))
          (return)))
      (throw-error -reference-error "no existing property found with the name " (:local multiname) " and one could not be created"))
    
    
    (define (ordinary-write (o object) (limit class) (multiname multiname) (env environment-opt) (new-value object) (create-if-missing boolean) (phase (tag run)))
            (tag none ok)
      (const m-base instance-property-opt (find-base-instance-property limit multiname write))
      (rwhen (not-in m-base (tag none) :narrow-true)
        (write-instance-property o limit m-base new-value phase)
        (return ok))
      (rwhen (/= limit (object-type o) class)
        (return none))
      (const m property-opt (find-archetype-property o multiname write true))
      (case m
        (:select (tag none)
          (reserve qname)
          (when (and create-if-missing
                     (in o (union simple-instance date reg-exp package) :narrow-true)
                     (not (& sealed o))
                     (some multiname qname (= (& namespace qname) public namespace) :define-true))
            (note "Before trying to create a new dynamic property named " (:local qname)
                  ", check that there is no read-only fixed property with the same name.")
            (rwhen (and (in (find-base-instance-property (object-type o) (list-set qname) read) (tag none))
                        (in (find-archetype-property o (list-set qname) read true) (tag none)))
              (create-dynamic-property o qname false true new-value)
              (return ok)))
          (return none))
        (:narrow singleton-property
          (write-singleton-property m new-value phase)
          (return ok))
        (:narrow instance-property
          (rwhen (or (not-in o class :narrow-false) (in env (tag none) :narrow-false))
            (throw-error -reference-error "cannot write an instance property without supplying an instance"))
          (const this object (read-implicit-this env))
          (write-instance-property this (object-type this) m new-value phase)
          (return ok))))
    
    
    (%text :comment "The caller must make sure that the created property does not already exist and does not conflict with any other property.")
    (define (create-dynamic-property (o (union simple-instance date reg-exp package)) (qname qualified-name) (sealed boolean) (enumerable boolean) (new-value object))
            void
      (const dv dynamic-var (new dynamic-var new-value sealed))
      (&= local-bindings o (set+ (& local-bindings o) (list-set (new local-binding qname read-write false enumerable dv)))))
    

    (define (write-instance-property (this object) (c class) (m-base instance-property) (new-value object) (phase (tag run)))
            void
      (const m instance-property (get-derived-instance-property c m-base write))
      (case m
        (:narrow instance-variable
          (const s slot (find-slot this m))
          (const coerced-value object (coerce new-value (&opt type m)))
          (rwhen (and (& immutable m) (not-in (& value s) (tag none)))
            (throw-error -reference-error "cannot initialise a " (:character-literal "const") " instance variable twice"))
          (&= value s coerced-value))
        (:select instance-method
          (throw-error -reference-error "cannot write to an instance method"))
        (:narrow instance-getter
          (bottom (:local m) " cannot be an " (:type instance-getter) " because these are only represented as read-only properties."))
        (:narrow instance-setter
          ((& call m) this new-value phase))))
    
    
    (define (write-singleton-property (m singleton-property) (new-value object) (phase (tag run))) void
      (case m
        (:select (tag forbidden)
          (throw-error -reference-error "cannot access a property defined in a scope outside the current region if any block inside the current region shadows it"))
        (:narrow variable (exec (write-variable m new-value false)))
        (:narrow dynamic-var
          (&= value m new-value))
        (:narrow getter
          (bottom (:local m) " cannot be a " (:type getter) " because these are only represented as read-only properties."))
        (:narrow setter
          (const env environment-opt (& env m))
          (assert (not-in env (tag none) :narrow-true)
            "All instances are resolved for the " (:tag run) " phase, so " (:assertion) ".")
          ((& call m) new-value env phase))))
    
    
    (%heading (2 :semantics) "Deleting")
    (%text :comment "If " (:local r) " is a " (:type reference) ", " (:global-call delete-reference r) " deletes it. If "
           (:local r) " is an " (:type object) ", this function signals an error in strict mode or returns " (:tag true) " in non-strict mode. "
           (:global delete-reference) " is never called from a constant expression.")
    (define (delete-reference (r obj-or-ref) (strict boolean) (phase (tag run))) boolean
      (var result boolean-opt)
      (case r
        (:select object
          (if strict
            (throw-error -reference-error "a non-reference is not a valid target for " (:character-literal "delete") " in strict mode")
            (<- result true)))
        (:narrow lexical-reference
          (<- result (lexical-delete (& env r) (& variable-multiname r) phase)))
        (:narrow dot-reference
          (<- result ((& delete (& limit r)) (& base r) (& limit r) (& multiname r) none phase)))
        (:narrow bracket-reference
          (<- result ((& bracket-delete (& limit r)) (& base r) (& limit r) (& args r) phase))))
      (if (not-in result (tag none) :narrow-true)
        (return result)
        (return true)))
    
    
    (define (ordinary-bracket-delete (o object) (limit class) (args (vector object)) (phase (tag run))) boolean-opt
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const qname qualified-name (object-to-qualified-name (nth args 0) phase))
      (return ((& delete limit) o limit (list-set qname) none phase)))
    
    
    (define (lexical-delete (env environment) (multiname multiname) (phase (tag run))) boolean
      (var i integer 0)
      (while (< i (length env))
        (const frame frame (nth env i))
        (var result boolean-opt none)
        (case frame
          (:narrow (union package class)
            (const limit class (object-type frame))
            (<- result ((& delete limit) frame limit multiname env phase)))
          (:narrow (union parameter-frame local-frame)
            (when (not-in (find-local-singleton-property frame multiname write) (tag none))
              (<- result false)))
          (:narrow with-frame
            (const value object-opt (& value frame))
            (rwhen (in value (tag none) :narrow-false)
              (throw-error -uninitialized-error
                "cannot read a " (:character-literal "with") " statement" :apostrophe "s frame before that statement" :apostrophe
                "s expression has been evaluated"))
            (const limit class (object-type value))
            (<- result ((& delete limit) value limit multiname env phase))))
        (rwhen (not-in result (tag none) :narrow-true)
          (return result))
        (<- i (+ i 1)))
      (return true))
    
    
    (define (ordinary-delete (o object) (limit class) (multiname multiname) (env environment-opt) (phase (tag run) :unused))
            boolean-opt
      (rwhen (not-in (find-base-instance-property limit multiname write) (tag none))
        (return false))
      (rwhen (/= limit (object-type o) class)
        (return none))
      (const m property-opt (find-archetype-property o multiname write true))
      (case m
        (:select (tag none) (return none))
        (:select (tag forbidden)
          (throw-error -reference-error "cannot access a property defined in a scope outside the current region if any block inside the current region shadows it"))
        (:select (union variable getter setter) (return false))
        (:narrow dynamic-var
          (cond
           ((& sealed m) (return false))
           (nil
            (&= local-bindings (assert-in o binding-object)
                (map (& local-bindings (assert-in o binding-object))
                     b b (or (set-not-in (& qname b) multiname) (/= (& content b) m singleton-property))))
            (return true))))
        (:narrow instance-property
          (rwhen (or (not-in o class :narrow-false) (in env (tag none) :narrow-false))
            (return false))
          (exec (read-implicit-this env))
          (return false))))
    
    
    (%heading (2 :semantics) "Enumerating")
    
    (define (ordinary-enumerate (o object)) (list-set object)
      (const e1 (list-set object) (enumerate-instance-properties (object-type o)))
      (const e2 (list-set object) (enumerate-archetype-properties o))
      (return (set+ e1 e2)))
    
    
    (define (enumerate-instance-properties (c class)) (list-set object)
      (var e (list-set object) (list-set-of object))
      (for-each (& instance-properties c) m
        (when (&opt enumerable m)
          (<- e (set+ e (map (&opt multiname m) qname (& id qname) (= (& namespace qname) public namespace))))))
      (const super class-opt (& super c))
      (if (in super (tag none) :narrow-false)
        (return e)
        (return (set+ e (enumerate-instance-properties super)))))
    
    
    (define (enumerate-archetype-properties (o object)) (list-set object)
      (var e (list-set object) (list-set-of object))
      (for-each (set+ (list-set o) (archetypes o)) a
        (when (in a binding-object :narrow-true)
          (<- e (set+ e (enumerate-singleton-properties a)))))
      (return e))
    
    
    (define (enumerate-singleton-properties (o binding-object)) (list-set object)
      (var e (list-set object) (list-set-of object))
      (for-each (& local-bindings o) b
        (when (and (& enumerable b) (= (& namespace (& qname b)) public namespace))
          (<- e (set+ e (list-set-of object (& id (& qname b)))))))
      (when (in o class :narrow-true)
        (const super class-opt (& super o))
        (when (not-in super (tag none) :narrow-true)
          (<- e (set+ e (enumerate-singleton-properties super)))))
      (return e))
    
    
    
    (%heading (2 :semantics) "Calling Instances")
    (define (call (this object) (a object) (args (vector object)) (phase phase)) object
      (case a
        (:select (union undefined null boolean general-number char16 string namespace compound-attribute date reg-exp package)
          (throw-error -type-error))
        (:narrow class
          (return ((& call a) this a args phase)))
        (:narrow simple-instance
          (const f (union (-> (object simple-instance (vector object) phase) object) (tag none)) (& call a))
          (rwhen (in f (tag none) :narrow-false)
            (throw-error -type-error))
          (return (f this a args phase)))
        (:narrow method-closure
          (const m instance-method (& method a))
          (return ((& call m) (& this a) args phase)))))
    
    
    (define (ordinary-call (this object :unused) (c class) (args (vector object)) (phase phase :unused)) object
      (note "This function can be used in a constant expression.")
      (rwhen (not (& complete c))
        (throw-error -constant-error "cannot call a class before its definition has been compiled"))
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (return (coerce (nth args 0) c)))
    
    
    (define (same-as-construct (this object :unused) (c class) (args (vector object)) (phase phase)) object
      (return (construct c args phase)))
    
    
    (%heading (2 :semantics) "Creating Instances")
    (define (construct (a object) (args (vector object)) (phase phase)) object
      (case a
        (:select (union undefined null boolean general-number char16 string namespace compound-attribute method-closure date reg-exp package)
          (throw-error -type-error))
        (:narrow class
          (return ((& construct a) a args phase)))
        (:narrow simple-instance
          (const f (union (-> (simple-instance (vector object) phase) object) (tag none)) (& construct a))
          (rwhen (in f (tag none) :narrow-false)
            (throw-error -type-error))
          (return (f a args phase)))))
    
    
    (define (ordinary-construct (c class) (args (vector object)) (phase phase)) object
      (rwhen (not (& complete c))
        (throw-error -constant-error "cannot construct an instance of a class before its definition has been compiled"))
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error "a class constructor call is not a constant expression because it evaluates to a new object each time it is evaluated"))
      (const this simple-instance (create-simple-instance c (&opt prototype c) none none none))
      (call-init this c args phase)
      (return this))
    
    
    (define (create-simple-instance (c class)
                                    (archetype object-opt)
                                    (call (union (-> (object simple-instance (vector object) phase) object) (tag none)))
                                    (construct (union (-> (simple-instance (vector object) phase) object) (tag none)))
                                    (env environment-opt))
            simple-instance
      (var slots (list-set slot) (list-set-of slot))
      (for-each (ancestors c) s
        (for-each (& instance-properties s) m
          (when (in m instance-variable :narrow-true)
            (const slot slot (new slot m (&opt default-value m)))
            (<- slots (set+ slots (list-set slot))))))
      (return (new simple-instance (list-set-of local-binding) archetype (not (& dynamic c)) c slots call construct env)))
    
    
    (define (call-init (this simple-instance) (c class-opt) (args (vector object)) (phase (tag run))) void
      (var init (union (-> (simple-instance (vector object) (tag run)) void) (tag none)) none)
      (when (not-in c (tag none) :narrow-true)
        (<- init (& init c)))
      (cond
       ((not-in init (tag none) :narrow-true) (init this args phase))
       (nil (rwhen (nonempty args)
              (throw-error -argument-error "the default constructor does not take any arguments")))))

    
    (%heading (2 :semantics) "Adding Local Definitions")
    (define (define-singleton-property (env environment) (id string) (namespaces (list-set namespace)) (override-mod override-modifier) (explicit boolean)
              (accesses access-set) (m singleton-property))
            multiname
      (const inner-frame non-with-frame (assert-not-in (nth env 0) with-frame))
      (rwhen (not-in override-mod (tag none))
        (throw-error -attribute-error "a local definition cannot have the " (:character-literal "override") " attribute"))
      (rwhen (and explicit (not-in inner-frame package))
        (throw-error -attribute-error "the " (:character-literal "explicit") " attribute can only be used at the top level of a package"))
      (var namespaces2 (list-set namespace) namespaces)
      (when (empty namespaces2)
        (<- namespaces2 (list-set public)))
      (const multiname multiname (map namespaces2 ns (new qualified-name ns id)))
      (const regional-env (vector frame) (get-regional-environment env))
      (rwhen (some (& local-bindings inner-frame) b (and (set-in (& qname b) multiname) (accesses-overlap (& accesses b) accesses)))
        (throw-error -definition-error "duplicate definition in the same scope"))
      (rwhen (and (in inner-frame class :narrow-true) (= id (& name inner-frame) string))
        (throw-error -definition-error "a " (:character-literal "static") " property of a class cannot have the same name as the class, regardless of the namespace"))
      (for-each (subseq regional-env 1) frame
        (rwhen (and (not-in frame with-frame :narrow-true)
                    (some (& local-bindings frame) b (and (set-in (& qname b) multiname) (accesses-overlap (& accesses b) accesses)
                                                          (not-in (& content b) (tag forbidden)))))
          (throw-error -definition-error "this definition would shadow a property defined in an outer scope within the same region")))
      (const new-bindings (list-set local-binding) (map multiname qname (new local-binding qname accesses explicit true m)))
      (&= local-bindings inner-frame (set+ (& local-bindings inner-frame) new-bindings))
      (note "Mark the bindings of " (:local multiname) " as " (:tag forbidden)
            " in all non-innermost frames in the current region if they haven" :apostrophe "t been marked as such already.")
      (const new-forbidden-bindings (list-set local-binding) (map multiname qname (new local-binding qname accesses true true forbidden)))
      (for-each (subseq regional-env 1) frame
        (assert (not-in frame class) "Since " (:assertion) " here, a " (:type class) " frame never gets a " (:tag forbidden) " binding.")
        (when (not-in frame with-frame :narrow-true)
          (&= local-bindings frame (set+ (& local-bindings frame) new-forbidden-bindings))))
      (return multiname))
      
    
    (%text :comment (:global-call define-hoisted-var env id initial-value) " defines a hoisted variable with the name " (:local id)
           " in the environment " (:local env) ". Hoisted variables are hoisted to the package or enclosing function scope. "
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
      (const qname qualified-name (new qualified-name public id))
      (const regional-env (vector frame) (get-regional-environment env))
      (var regional-frame frame (nth regional-env (- (length regional-env) 1)))
      (assert (in regional-frame (union package parameter-frame) :narrow-true)
        (:local env) " is either a " (:type package) " or a " (:type parameter-frame)
        " because hoisting only occurs into package or function scope.")
      (var existing-bindings (list-set local-binding) (map (& local-bindings regional-frame) b b (= (& qname b) qname qualified-name)))
      (when (and (or (empty existing-bindings) (not-in initial-value (tag undefined)))
                 (in regional-frame parameter-frame) (>= (length regional-env) 2))
        (<- regional-frame (nth regional-env (- (length regional-env) 2)) :end-narrow)
        (<- existing-bindings (map (& local-bindings (assert-in regional-frame local-frame)) b b (= (& qname b) qname qualified-name))))
      (cond
       ((empty existing-bindings)
        (const v dynamic-var (new dynamic-var initial-value true))
        (&= local-bindings regional-frame (set+ (& local-bindings regional-frame) (list-set (new local-binding qname read-write false true v))))
        (return v))
       ((/= (length existing-bindings) 1)
        (throw-error -definition-error "a hoisted definition conflicts with a non-hoisted one"))
       (nil
        (const b local-binding (unique-elt-of existing-bindings))
        (const m singleton-property (& content b))
        (rwhen (or (not-in (& accesses b) (tag read-write)) (not-in m dynamic-var :narrow-false))
          (throw-error -definition-error "a hoisted definition conflicts with a non-hoisted one"))
        (note "At this point a hoisted binding of the same " (:character-literal "var") " already exists, so there is no need to create another one. "
              "Overwrite its initial value if the new definition is a " (:character-literal "function") " definition.")
        (when (not-in initial-value (tag undefined))
          (&= value m initial-value))
        (&= sealed m true)
        (&= local-bindings regional-frame (set- (& local-bindings regional-frame) (list-set b)))
        (&= local-bindings regional-frame (set+ (& local-bindings regional-frame) (list-set (set-field b enumerable true))))
        (return m))))
    
    
    (%heading (2 :semantics) "Adding Instance Definitions")
    
    (define (search-for-overrides (c class) (multiname multiname) (accesses access-set)) instance-property-opt
      (var m-base instance-property-opt none)
      (const s class-opt (& super c))
      (when (not-in s (tag none) :narrow-true)
        (for-each multiname qname
          (const m instance-property-opt (find-base-instance-property s (list-set qname) accesses))
          (cond
           ((in m-base (tag none) :narrow-false) (<- m-base m))
           ((and (not-in m (tag none) :narrow-true) (/= m m-base instance-property))
            (throw-error -definition-error "cannot override two separate superclass methods at the same time")))))
      (return m-base))
    
    
    (define (define-instance-property (c class) (cxt context) (id string) (namespaces (list-set namespace))
              (override-mod override-modifier) (explicit boolean) (m instance-property))
            instance-property-opt
      (rwhen explicit
        (throw-error -attribute-error "the " (:character-literal "explicit") " attribute can only be used at the top level of a package"))
      (const accesses access-set (instance-property-accesses m))
      (const requested-multiname multiname (map namespaces ns (new qualified-name ns id)))
      (const open-multiname multiname (map (& open-namespaces cxt) ns (new qualified-name ns id)))
      (var defined-multiname multiname)
      (var searched-multiname multiname)
      (cond
       ((empty requested-multiname)
        (<- defined-multiname (list-set (new qualified-name public id)))
        (<- searched-multiname open-multiname)
        (assert (set<= defined-multiname searched-multiname multiname) (:assertion) " because the " (:character-literal "public") " namespace is always open."))
       (nil
        (<- defined-multiname requested-multiname)
        (<- searched-multiname requested-multiname)))
      (const m-base instance-property-opt (search-for-overrides c searched-multiname accesses))
      (var m-overridden instance-property-opt none)
      (when (not-in m-base (tag none) :narrow-true)
        (<- m-overridden (get-derived-instance-property c m-base accesses))
        (quiet-assert (not-in m-overridden (tag none) :narrow-true))
        (<- defined-multiname (&opt multiname m-overridden))
        (rwhen (not (set<= requested-multiname defined-multiname multiname))
          (throw-error -definition-error "cannot extend the set of a property" :apostrophe "s namespaces when overriding it"))
        (var good-kind boolean)
        (case m
          (:select instance-variable (<- good-kind (in m-overridden instance-variable)))
          (:select instance-getter (<- good-kind (in m-overridden (union instance-variable instance-getter))))
          (:select instance-setter (<- good-kind (in m-overridden (union instance-variable instance-setter))))
          (:select instance-method (<- good-kind (in m-overridden instance-method))))
        (rwhen (not good-kind)
          (throw-error -definition-error
            "a method can override only another method, a variable can override only another variable, a getter can override only a getter or a variable, and "
            "a setter can override only a setter or a variable"))
        (rwhen (& final m-overridden)
          (throw-error -definition-error "cannot override a " (:character-literal "final") " property")))
      (rwhen (some (& instance-properties c) m2 (and (nonempty (set* (&opt multiname m2) defined-multiname)) (accesses-overlap (instance-property-accesses m2) accesses)))
        (throw-error -definition-error "duplicate definition in the same scope"))
      (case override-mod
        (:select (tag none)
          (rwhen (not-in m-base (tag none))
            (throw-error -definition-error
              "a definition that overrides a superclass" :apostrophe "s property must be marked with the " (:character-literal "override") " attribute"))
          (rwhen (not-in (search-for-overrides c open-multiname accesses) (tag none))
            (throw-error -definition-error
              "this definition is hidden by one in a superclass when accessed without a namespace qualifier; "
              "in the rare cases where this is intentional, use the " (:character-literal "override(false)") " attribute")))
        (:select (tag false)
          (rwhen (not-in m-base (tag none))
            (throw-error -definition-error
              "this definition is marked with " (:character-literal "override(false)") " but it overrides a superclass" :apostrophe "s property")))
        (:select (tag true)
          (rwhen (in m-base (tag none))
            (throw-error -definition-error
              "this definition is marked with " (:character-literal "override") " or " (:character-literal "override(true)")
              " but it doesn" :apostrophe "t override a superclass" :apostrophe "s property")))
        (:select (tag undefined)))
      (&const= multiname m defined-multiname)
      (&= instance-properties c (set+ (& instance-properties c) (list-set m)))
      (return m-overridden))
    
    
    (%heading (2 :semantics) "Instantiation")
    
    (define (instantiate-function (uf uninstantiated-function) (env environment)) simple-instance
      (const c class (& type uf))
      (const i simple-instance (create-simple-instance c (&opt prototype c) (& call uf) (& construct uf) env))
      (dot-write i (list-set (new qualified-name public "length")) (real-to-float64 (& length uf)) run)
      (when (= c -prototype-function class)
        (const prototype object (construct -object (vector-of object) run))
        (dot-write prototype (list-set (new qualified-name public "constructor")) i run)
        (dot-write i (list-set (new qualified-name public "prototype")) prototype run))
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
    
    
    (define (instantiate-property (m singleton-property) (env environment)) singleton-property
      (case m
        (:select (tag forbidden) (return m))
        (:narrow variable
          (assert (in (& setup m) (tag none))
            (:assertion) " because " (:action setup) " must have been called on a frame before that frame can be instantiated.")
          (var value variable-value (& value m))
          (when (in value uninstantiated-function :narrow-true)
            (<- value (instantiate-function value env) :end-narrow))
          (return (new variable (&opt type m) value (& immutable m) none (& initializer m) env)))
        (:narrow dynamic-var
          (var value (union object uninstantiated-function) (& value m))
          (when (in value uninstantiated-function :narrow-true)
            (<- value (instantiate-function value env) :end-narrow))
          (return (new dynamic-var value (& sealed m))))
        (:narrow getter
          (case (& env m)
            (:select environment (return m))
            (:select (tag none) (return (new getter (& call m) env)))))
        (:narrow setter
          (case (& env m)
            (:select environment (return m))
            (:select (tag none) (return (new setter (& call m) env)))))))
    
    
    (deftuple property-translation
      (from singleton-property)
      (to singleton-property))
    
    (define (instantiate-local-frame (frame local-frame) (env environment))
            local-frame
      (const instantiated-frame local-frame (new local-frame (list-set-of local-binding)))
      (const properties (list-set singleton-property)
        (map (& local-bindings frame) b (& content b)))
      (const property-translations (list-set property-translation)
        (map properties m (new property-translation m (instantiate-property m (cons instantiated-frame env)))))
      (function (translate-property (m singleton-property)) singleton-property
        (const mi property-translation (unique-elt-of property-translations mi (= (& from mi) m singleton-property)))
        (return (& to mi)))
      (&= local-bindings instantiated-frame (map (& local-bindings frame) b (set-field b content (translate-property (& content b)))))
      (return instantiated-frame))
    
    
    (define (instantiate-parameter-frame (frame parameter-frame) (env environment) (singular-this object-opt))
            parameter-frame
      (assert (= (& superconstructor-called frame) (not-in (& kind frame) (tag constructor-function)) boolean)
        (:expr boolean (& superconstructor-called frame)) " must be " (:tag true) " if and only if "
        (:expr function-kind (& kind frame)) " is not " (:tag constructor-function) ".")
      (const instantiated-frame parameter-frame (new parameter-frame (list-set-of local-binding) (& kind frame) (& handling frame)
                                                     (& calls-superconstructor frame) (& superconstructor-called frame)
                                                     singular-this :uninit :uninit (&opt return-type frame)))
      (note (:local properties) " will contain the set of all " (:type singleton-property) " records found in the " (:local frame) ".")
      (var properties (list-set singleton-property)
        (map (& local-bindings frame) b (& content b)))
      (note "If any of the parameters (including the rest parameter) are anonymous, their bindings will not be present in "
            (:expr (list-set local-binding) (& local-bindings frame)) ". In this situation, the following steps add their "
            (:type singleton-property) " records to " (:local properties) ".")
      (for-each (&opt parameters frame) p
        (<- properties (set+ properties (list-set-of singleton-property (& var p)))))
      (const rest variable-opt (&opt rest frame))
      (when (not-in rest (tag none) :narrow-true)
        (<- properties (set+ properties (list-set-of singleton-property rest))))
      (const property-translations (list-set property-translation)
        (map properties m (new property-translation m (instantiate-property m (cons instantiated-frame env)))))
      (function (translate-property (m singleton-property)) singleton-property
        (const mi property-translation (unique-elt-of property-translations mi (= (& from mi) m singleton-property)))
        (return (& to mi)))
      (&= local-bindings instantiated-frame (map (& local-bindings frame) b (set-field b content (translate-property (& content b)))))
      (&= parameters instantiated-frame (map (&opt parameters frame) op
                                             (new parameter (assert-in (translate-property (& var op)) (union variable dynamic-var)) (& default op))))
      (if (in rest (tag none) :narrow-false)
        (&= rest instantiated-frame none)
        (&= rest instantiated-frame (assert-in (translate-property rest) variable)))
      (return instantiated-frame))
    
    
    
    (%heading (2 :semantics) "Sealing")
    
    (define (seal-object (o object)) void
      (when (in o (union simple-instance reg-exp date package) :narrow-true)
        (&= sealed o true)))
    
    
    (define (seal-all-local-properties (o object)) void
      (when (in o binding-object :narrow-true)
        (for-each (& local-bindings o) b
          (const m singleton-property (& content b))
          (when (in m dynamic-var :narrow-true)
            (&= sealed m true)))))
    
    
    (define (seal-local-property (o object) (qname qualified-name)) void
      (const c class (object-type o))
      (when (and (in (find-base-instance-property c (list-set qname) read) (tag none))
                 (in (find-base-instance-property c (list-set qname) write) (tag none))
                 (in o binding-object :narrow-true))
        (const matching-properties (list-set singleton-property) (map (& local-bindings o) b (& content b) (= (& qname b) qname qualified-name)))
        (for-each matching-properties m
          (when (in m dynamic-var :narrow-true)
            (&= sealed m true)))))
    
    
    
    (%heading (2 :semantics) "Standard Class Utilities")
    
    (define (default-arg (args (vector object)) (n integer) (default object)) object
      (rwhen (>= n (length args))
        (return default))
      (const arg object (nth args n))
      (if (in arg (tag undefined))
        (return default)
        (return arg)))
    
    
    (define (std-const-binding (qname qualified-name) (type (delay class)) (value object)) local-binding
      (return (new local-binding
                qname read-write false false
                (new variable type value true none none :uninit))))
    
    
    (define (std-explicit-const-binding (qname qualified-name) (type (delay class)) (value object)) local-binding
      (return (new local-binding
                qname read-write true false
                (new variable type value true none none :uninit))))
    
    
    (define (std-var-binding (qname qualified-name) (type (delay class)) (value object)) local-binding
      (return (new local-binding
                qname read-write false false
                (new variable type value false none none :uninit))))
    
    
    (define (std-function (qname qualified-name) (call (-> (object simple-instance (vector object) phase) object)) (length integer)) local-binding
      (const slots (list-set slot) (list-set (new slot ivar-function-length (real-to-float64 length))))
      (const f simple-instance (new simple-instance (list-set-of local-binding) (delay -function-prototype) true -function slots call none none))
      (return (new local-binding
                qname read-write false false
                (new variable -function f true none none :uninit))))
    
    
    (%text :comment (:global-call std-reserve qname archetype) " is used during the creation of system objects. It returns an alias of the local binding of "
           (:local qname) " in " (:local archetype)
           ", which should be the archetype of the object being created. The alias that " (:global std-reserve) " defines serves to prevent " (:local qname)
           " from being later redefined by users in the object being created while at the same time retaining the definition of " (:local qname)
           " that would normally be inherited from " (:local archetype) ".")
    (define (std-reserve (qname qualified-name) (archetype simple-instance)) local-binding
      (const matching-bindings (list-set local-binding) (map (& local-bindings archetype) b b (= (& qname b) qname qualified-name)))
      (return (unique-elt-of matching-bindings)))
     
    
    
    (%heading 1 "Expressions")
    (grammar-argument :beta allow-in no-in)
    
    
    (%heading (2 :semantics) "Terminal Actions")
    
    (declare-action name $identifier string :action nil
      (terminal-action name $identifier identity))
    (declare-action value $number general-number :action nil
      (terminal-action value $number identity))
    (declare-action value $string string :action nil
      (terminal-action value $string identity))
    (declare-action body $regular-expression string :action nil
      (terminal-action body $regular-expression first))
    (declare-action flags $regular-expression string :action nil
      (terminal-action flags $regular-expression second))
    (%print-actions)
    
    
    (%heading 2 "Identifiers")
    (rule :identifier ((name string))
      (production :identifier ($identifier) identifier-identifier (name (name $identifier)))
      (production :identifier (get) identifier-get (name "get"))
      (production :identifier (set) identifier-set (name "set"))
      (? js2 (production :identifier (include) identifier-include (name "include"))))
    (%print-actions)
    
    (%heading 2 "Qualified Identifiers")
    (rule :simple-qualified-identifier ((open-namespaces (writable-cell (list-set namespace)))
                                        (strict (writable-cell boolean))
                                        (validate (-> (context environment) void))
                                        (setup (-> () void))
                                        (eval (-> (environment phase) multiname)))
      (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier
        ((validate cxt (env :unused))
         (action<- (open-namespaces :simple-qualified-identifier 0) (& open-namespaces cxt))
         (action<- (strict :simple-qualified-identifier 0) (& strict cxt)))
        ((setup))
        ((eval (env :unused) (phase :unused))
         (return (map (open-namespaces :simple-qualified-identifier 0) ns (new qualified-name ns (name :identifier))))))
      (production :simple-qualified-identifier (:identifier \:\: :identifier) simple-qualified-identifier-identifier-qualifier
        ((validate cxt (env :unused))
         (action<- (open-namespaces :simple-qualified-identifier 0) (& open-namespaces cxt)))
        ((setup))
        ((eval env phase)
         (const multiname multiname (map (open-namespaces :simple-qualified-identifier 0) ns (new qualified-name ns (name :identifier 1))))
         (const a object (lexical-read env multiname phase))
         (rwhen (not-in a namespace :narrow-false)
           (throw-error -type-error "the qualifier must be a namespace"))
         (return (list-set (new qualified-name a (name :identifier 2))))))
      (production :simple-qualified-identifier (:reserved-namespace \:\: :identifier) simple-qualified-identifier-reserved-namespace-qualifier
        ((validate cxt env) ((validate :reserved-namespace) cxt env))
        ((setup) ((setup :reserved-namespace)))
        ((eval env phase)
         (const q namespace ((eval :reserved-namespace) env phase))
         (return (list-set (new qualified-name q (name :identifier)))))))
    
    (rule :expression-qualified-identifier ((strict (writable-cell boolean))
                                            (validate (-> (context environment) void))
                                            (setup (-> () void))
                                            (eval (-> (environment phase) multiname)))
      (production :expression-qualified-identifier (:paren-expression \:\: :identifier) expression-qualified-identifier-identifier
        ((validate cxt env)
         (action<- (strict :expression-qualified-identifier 0) (& strict cxt))
         ((validate :paren-expression) cxt env))
        ((setup) ((setup :paren-expression)))
        ((eval env phase)
         (const q object (read-reference ((eval :paren-expression) env phase) phase))
         (rwhen (not-in q namespace :narrow-false) (throw-error -type-error "the qualifier must be a namespace"))
         (return (list-set (new qualified-name q (name :identifier)))))))
    
    (rule :qualified-identifier ((strict (writable-cell boolean))
                                 (validate (-> (context environment) void))
                                 (setup (-> () void))
                                 (eval (-> (environment phase) multiname)))
      (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple
        ((validate cxt env)
         (action<- (strict :qualified-identifier 0) (& strict cxt))
         ((validate :simple-qualified-identifier) cxt env))
        ((setup) :forward)
        ((eval env phase) :forward-result))
      (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression
        ((validate cxt env)
         (action<- (strict :qualified-identifier 0) (& strict cxt))
         ((validate :expression-qualified-identifier) cxt env))
        ((setup) :forward)
        ((eval env phase) :forward-result)))
    (%print-actions ("Validation" open-namespaces strict validate) ("Setup" setup) ("Evaluation" eval))
    
    
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
      (production :primary-expression ($number) primary-expression-number
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (value $number))))
      (production :primary-expression ($string) primary-expression-string
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (value $string))))
      (production :primary-expression (this) primary-expression-this
        ((validate cxt env)
         (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
         (cond
          ((in frame (tag none) :narrow-false)
           (rwhen (& strict cxt)
             (throw-error -syntax-error (:character-literal "this") " can be used outside a function only in non-strict mode")))
          ((in (& kind frame) (tag plain-function))
           (throw-error -syntax-error "this function does not define " (:character-literal "this")))))
        ((setup) :forward)
        ((eval env phase)
         (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
         (rwhen (in frame (tag none) :narrow-false)
           (return (get-package-frame env)))
         (assert (not-in (& kind frame) (tag plain-function))
           (:action validate) " ensured that " (:assertion) " at this point.")
         (const this object-opt (& this frame))
         (rwhen (in this (tag none) :narrow-false)
           (assert (in phase (tag compile)) "If " (:action validate) " passed, " (:local this) " can be uninitialised only when " (:assertion) ".")
           (throw-error -constant-error "a constant expression cannot read an uninitialised " (:local this) " parameter"))
         (rwhen (not (& superconstructor-called frame))
           (throw-error -uninitialized-error "can" :apostrophe "t access " (:character-literal "this") " from within a constructor before the superconstructor has been called"))
         (return this)))
      (production :primary-expression ($regular-expression) primary-expression-regular-expression
        ((validate (cxt :unused) (env :unused)))
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (append (body $regular-expression) "#" (flags $regular-expression))))) ;*****
      (production :primary-expression (:reserved-namespace) primary-expression-reserved-namespace
        ((validate cxt env) ((validate :reserved-namespace) cxt env))
        ((setup) :forward)
        ((eval env phase) (return ((eval :reserved-namespace) env phase))))
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
    
    (rule :reserved-namespace ((validate (-> (context environment) void)) (setup (-> () void)) (eval (-> (environment phase) namespace)))
      (production :reserved-namespace (public) reserved-namespace-public
        ((validate (cxt :unused) (env :unused)))
        ((setup))
        ((eval (env :unused) (phase :unused)) (return public)))
      (production :reserved-namespace (private) reserved-namespace-private
        ((validate (cxt :unused) env)
         (rwhen (in (get-enclosing-class env) (tag none))
           (throw-error -syntax-error (:character-literal "private") " is meaningful only inside a class")))
        ((setup))
        ((eval env (phase :unused))
         (const c class-opt (get-enclosing-class env))
         (assert (not-in c (tag none) :narrow-true)
           (:action validate) " already ensured that " (:assertion) ".")
         (return (&opt private-namespace c)))))

    (rule :paren-expression ((validate (-> (context environment) void)) (setup (-> () void)) (eval (-> (environment phase) obj-or-ref)))
      (production :paren-expression (\( (:assignment-expression allow-in) \)) paren-expression-assignment-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) :forward-result)))
    
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
         (var kind static-function-kind plain-function)
         (when (and (not (& strict cxt)) (plain :function-common))
           (<- kind unchecked-function))
         (action<- (f :function-expression 0) ((validate-static-function :function-common) cxt env kind)))
        ((setup) ((setup :function-common)))
        ((eval env phase)
         (rwhen (in phase (tag compile))
           (throw-error -constant-error "a " (:character-literal "function") " expression is not a constant expression because it can evaluate to different values"))
         (return (instantiate-function (f :function-expression 0) env))))
      (production :function-expression (function :identifier :function-common) function-expression-named
        ((validate cxt env)
         (const v variable (new variable -function none true none busy :uninit))
         (const b local-binding (new local-binding (new qualified-name public (name :identifier)) read-write false true v))
         (const compile-frame local-frame (new local-frame (list-set b)))
         (var kind static-function-kind plain-function)
         (when (and (not (& strict cxt)) (plain :function-common))
           (<- kind unchecked-function))
         (action<- (f :function-expression 0) ((validate-static-function :function-common) cxt (cons compile-frame env) kind)))
        ((setup) ((setup :function-common)))
        ((eval env phase)
         (rwhen (in phase (tag compile))
           (throw-error -constant-error "a " (:character-literal "function") " expression is not a constant expression because it can evaluate to different values"))
         (const v variable (new variable -function none true none none :uninit))
         (const b local-binding (new local-binding (new qualified-name public (name :identifier)) read-write false true v))
         (const runtime-frame local-frame (new local-frame (list-set b)))
         (const f simple-instance (instantiate-function (f :function-expression 0) (cons runtime-frame env)))
         (&= value v f)
         (return f))))
    (%print-actions ("Validation" f validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Object Literals")
    (rule :object-literal ((validate (-> (context environment) void)) (setup (-> () void))
                           (eval (-> (environment phase) obj-or-ref)))
      (production :object-literal (\{ :field-list \}) object-literal-list
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "an object literal is not a constant expression because it evaluates to a new object each time it is evaluated"))
         (const o object (construct -object (vector-of object) phase))
         ((eval :field-list) env o phase)
         (return o))))
    
    
    (rule :field-list ((validate (-> (context environment) void)) (setup (-> () void))
                       (eval (-> (environment object (tag run)) void)))
      (production :field-list () field-list-empty
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env o phase) :forward))
      (production :field-list (:nonempty-field-list) field-list-nonempty
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env o phase) :forward)))
    
    
    (rule :nonempty-field-list ((validate (-> (context environment) void)) (setup (-> () void))
                                (eval (-> (environment object (tag run)) void)))
      (production :nonempty-field-list (:literal-field) nonempty-field-list-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env o phase) :forward))
      (production :nonempty-field-list (:literal-field \, :nonempty-field-list) nonempty-field-list-more
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env o phase) :forward)))
    
    
    (rule :literal-field ((validate (-> (context environment) void))
                          (setup (-> () void))
                          (eval (-> (environment object (tag run)) void)))
      (production :literal-field (:field-name \: (:assignment-expression allow-in)) literal-field-assignment-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env o phase)
         (const multiname multiname ((eval :field-name) env phase))
         (const value object (read-reference ((eval :assignment-expression) env phase) phase))
         (dot-write o multiname value phase))))
    
    
    (rule :field-name ((validate (-> (context environment) void)) (setup (-> () void))
                       (eval (-> (environment phase) multiname)))
      (production :field-name (:qualified-identifier) field-name-identifier
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :qualified-identifier) env phase))))
      (production :field-name ($string) field-name-string
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) phase) (return (list-set (object-to-qualified-name (value $string) phase)))))
      (production :field-name ($number) field-name-number
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) phase) (return (list-set (object-to-qualified-name (value $number) phase)))))
      (production :field-name (:paren-expression) field-name-paren-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :paren-expression) env phase) phase))
         (return (list-set (object-to-qualified-name a phase))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Array Literals")
    (rule :array-literal ((validate (-> (context environment) void)) (setup (-> () void))
                          (eval (-> (environment phase) obj-or-ref)))
      (production :array-literal ([ :element-list ]) array-literal-list
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "an array literal is not a constant expression because it evaluates to a new object each time it is evaluated"))
         (const o object (construct -array (vector-of object) phase))
         (const length integer ((eval :element-list) env 0 o phase))
         (write-array-private-length o length phase)
         (return o))))
    
    
    (rule :element-list ((validate (-> (context environment) void)) (setup (-> () void))
                         (eval (-> (environment integer object (tag run)) integer)))
      (production :element-list () element-list-none
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) length (o :unused) (phase :unused)) (return length)))
      (production :element-list (:literal-element) element-list-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env length o phase)
         ((eval :literal-element) env length o phase)
         (return (+ length 1))))
      (production :element-list (\, :element-list) element-list-hole
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env length o phase)
         (return ((eval :element-list) env (+ length 1) o phase))))
      (production :element-list (:literal-element \, :element-list) element-list-more
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env length o phase)
         ((eval :literal-element) env length o phase)
         (return ((eval :element-list) env (+ length 1) o phase)))))
    
    
    (rule :literal-element ((validate (-> (context environment) void))
                            (setup (-> () void))
                            (eval (-> (environment integer object (tag run)) void)))
      (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env length o phase)
         (const value object (read-reference ((eval :assignment-expression) env phase) phase))
         (index-write o length value phase))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Super Expressions")
    (rule :super-expression ((validate (-> (context environment) void)) (setup (-> () void))
                             (eval (-> (environment phase) obj-optional-limit)))
      (production :super-expression (super) super-expression-super
        ((validate (cxt :unused) env)
         (const c class-opt (get-enclosing-class env))
         (rwhen (in c (tag none) :narrow-false)
           (throw-error -syntax-error "a " (:character-literal "super") " expression is meaningful only inside a class"))
         (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
         (rwhen (or (in frame (tag none) :narrow-false) (in (& kind frame) static-function-kind))
           (throw-error -syntax-error "a " (:character-literal "super") " expression without an argument is meaningful only inside an instance method or a constructor"))
         (rwhen (in (& super c) (tag none))
           (throw-error -syntax-error "a " (:character-literal "super") " expression is meaningful only if the enclosing class has a superclass")))
        ((setup) :forward)
        ((eval env phase)
         (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
         (assert (and (not-in frame (tag none) :narrow-true) (not-in (& kind frame) static-function-kind))
           (:action validate) " ensured that " (:assertion) " at this point.")
         (const this object-opt (& this frame))
         (rwhen (in this (tag none) :narrow-false)
           (assert (in phase (tag compile)) "If " (:action validate) " passed, " (:local this) " can be uninitialised only when " (:assertion) ".")
           (throw-error -constant-error "a constant expression cannot read an uninitialised " (:local this) " parameter"))
         (rwhen (not (& superconstructor-called frame))
           (throw-error -uninitialized-error "can" :apostrophe "t access " (:character-literal "super") " from within a constructor before the superconstructor has been called"))
         (return (make-limited-instance this (assert-not-in (get-enclosing-class env) (tag none)) phase))))
      (production :super-expression (super :paren-expression) super-expression-super-paren-expression
        ((validate cxt env)
         (const c class-opt (get-enclosing-class env))
         (rwhen (in c (tag none) :narrow-false)
           (throw-error -syntax-error "a " (:character-literal "super") " expression is meaningful only inside a class"))
         (rwhen (in (& super c) (tag none))
           (throw-error -syntax-error "a " (:character-literal "super") " expression is meaningful only if the enclosing class has a superclass"))
         ((validate :paren-expression) cxt env))
        ((setup) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :paren-expression) env phase))
         (return (make-limited-instance r (assert-not-in (get-enclosing-class env) (tag none)) phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (define (make-limited-instance (r obj-or-ref) (c class) (phase phase)) obj-optional-limit
      (const o object (read-reference r phase))
      (const limit class-opt (& super c))
      (assert (not-in limit (tag none) :narrow-true)
        (:action validate) " ensured that " (:local limit) " cannot be " (:tag none) " at this point.")
      (const coerced object (coerce o limit))
      (rwhen (in coerced (tag null))
        (return null))
      (return (new limited-instance coerced limit)))
    
    
    (%heading 2 "Postfix Expressions")
    (rule :postfix-expression ((validate (-> (context environment) void)) (setup (-> () void))
                               (eval (-> (environment phase) obj-or-ref)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) :forward-result))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) :forward-result))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) :forward-result)))
    
    (rule :attribute-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                 (eval (-> (environment phase) obj-or-ref)))
      (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const m multiname ((eval :simple-qualified-identifier) env phase))
         (return (new lexical-reference env m (strict :simple-qualified-identifier)))))
      (production :attribute-expression (:attribute-expression :property-operator) attribute-expression-property-operator
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :attribute-expression) env phase) phase))
         (return ((eval :property-operator) env a phase))))
      (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :attribute-expression) env phase))
         (const f object (read-reference r phase))
         (var base object)
         (case r
           (:select (union object lexical-reference) (<- base null))
           (:narrow (union dot-reference bracket-reference) (<- base (& base r))))
         (const args (vector object) ((eval :arguments) env phase))
         (return (call base f args phase)))))
    
    (rule :full-postfix-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                    (eval (-> (environment phase) obj-or-ref)))
      (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :primary-expression) env phase))))
      (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const m multiname ((eval :expression-qualified-identifier) env phase))
         (return (new lexical-reference env m (strict :expression-qualified-identifier)))))
      (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :full-new-expression) env phase))))
      (production :full-postfix-expression (:full-postfix-expression :property-operator) full-postfix-expression-property-operator
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :full-postfix-expression) env phase) phase))
         (return ((eval :property-operator) env a phase))))
      (production :full-postfix-expression (:super-expression :property-operator) full-postfix-expression-super-property-operator
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a obj-optional-limit ((eval :super-expression) env phase))
         (return ((eval :property-operator) env a phase))))
      (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const r obj-or-ref ((eval :full-postfix-expression) env phase))
         (const f object (read-reference r phase))
         (var base object)
         (case r
           (:select (union object lexical-reference) (<- base null))
           (:narrow (union dot-reference bracket-reference) (<- base (& base r))))
         (const args (vector object) ((eval :arguments) env phase))
         (return (call base f args phase))))
      (production :full-postfix-expression (:postfix-expression :no-line-break ++) full-postfix-expression-increment
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error (:character-literal "++") " cannot be used in a constant expression"))
         (const r obj-or-ref ((eval :postfix-expression) env phase))
         (const a object (read-reference r phase))
         (const b object (plus a phase))
         (const c object (add b 1.0 phase))
         (write-reference r c phase)
         (return b)))
      (production :full-postfix-expression (:postfix-expression :no-line-break --) full-postfix-expression-decrement
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error (:character-literal "--") " cannot be used in a constant expression"))
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
    
    (rule :full-new-subexpression ((validate (-> (context environment) void)) (setup (-> () void))
                                   (eval (-> (environment phase) obj-or-ref)))
      (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :primary-expression) env phase))))
      (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const m multiname ((eval :qualified-identifier) env phase))
         (return (new lexical-reference env m (strict :qualified-identifier)))))
      (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :full-new-expression) env phase))))
      (production :full-new-subexpression (:full-new-subexpression :property-operator) full-new-subexpression-property-operator
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :full-new-subexpression) env phase) phase))
         (return ((eval :property-operator) env a phase))))
      (production :full-new-subexpression (:super-expression :property-operator) full-new-subexpression-super-property-operator
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a obj-optional-limit ((eval :super-expression) env phase))
         (return ((eval :property-operator) env a phase)))))
    
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
        ((eval env phase) :forward-result))
      (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) :forward-result)))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Property Operators")
    (rule :property-operator ((validate (-> (context environment) void)) (setup (-> () void))
                              (eval (-> (environment obj-optional-limit phase) obj-or-ref)))
      (production :property-operator (\. :qualified-identifier) property-operator-qualified-identifier
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env base phase)
         (const m multiname ((eval :qualified-identifier) env phase))
         (case base
           (:narrow object (return (new dot-reference base (object-type base) m)))
           (:narrow limited-instance (return (new dot-reference (& instance base) (& limit base) m))))))
      (production :property-operator (:brackets) property-operator-brackets
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env base phase)
         (const args (vector object) ((eval :brackets) env phase))
         (case base
           (:narrow object (return (new bracket-reference base (object-type base) args)))
           (:narrow limited-instance (return (new bracket-reference (& instance base) (& limit base) args)))))))
    
    (rule :brackets ((validate (-> (context environment) void)) (setup (-> () void))
                     (eval (-> (environment phase) (vector object))))
      (production :brackets ([ ]) brackets-none
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (vector-of object))))
      (production :brackets ([ (:list-expression allow-in) ]) brackets-some
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval-as-list :list-expression) env phase))))
      (production :brackets ([ :expressions-with-rest ]) brackets-rest
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :expressions-with-rest) env phase)))))
    
    (rule :arguments ((validate (-> (context environment) void)) (setup (-> () void))
                      (eval (-> (environment phase) (vector object))))
      (production :arguments (\( \)) arguments-none
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return (vector-of object))))
      (production :arguments (:paren-list-expression) arguments-some
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval-as-list :paren-list-expression) env phase))))
      (production :arguments (\( :expressions-with-rest \)) arguments-rest
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :expressions-with-rest) env phase)))))
    
    (rule :expressions-with-rest ((validate (-> (context environment) void)) (setup (-> () void))
                                  (eval (-> (environment phase) (vector object))))
      (production :expressions-with-rest (:rest-expression) expressions-with-rest-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :rest-expression) env phase))))
      (production :expressions-with-rest ((:list-expression allow-in) \, :rest-expression) expressions-with-rest-more
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const args1 (vector object) ((eval-as-list :list-expression) env phase))
         (const args2 (vector object) ((eval :rest-expression) env phase))
         (return (append args1 args2)))))
    
    (rule :rest-expression ((validate (-> (context environment) void)) (setup (-> () void))
                            (eval (-> (environment phase) (vector object))))
      (production :rest-expression (\.\.\. (:assignment-expression allow-in)) rest-expression-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :assignment-expression) env phase) phase))
         (const length integer (read-length a phase))
         (var i integer 0)
         (var args (vector object) (vector-of object))
         (while (/= i length)
           (const arg object-opt (index-read a i phase))
           (rwhen (in arg (tag none) :narrow-false)
             (/* "An implementation may, at its discretion, either " (:keyword throw) " a " (:global -reference-error)
                 " or treat the hole as a missing argument, substituting the called function" :apostrophe "s default parameter value if there is one, "
                 (:tag undefined) " if the called function is unchecked, or " (:keyword throw) "ing an " (:global -argument-error) " exception otherwise. "
                 "An implementation must not replace such a hole with " (:tag undefined) " except when the called function is unchecked or happens to "
                 "have " (:tag undefined) " as its default parameter value.")
             (throw-error -reference-error))
           (<- args (append args (vector arg)))
           (<- i (+ i 1)))
         (return args))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Unary Operators")
    (rule :unary-expression ((strict (writable-cell boolean))
                             (validate (-> (context environment) void)) (setup (-> () void))
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
           (throw-error -constant-error (:character-literal "delete") " cannot be used in a constant expression"))
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
           (throw-error -constant-error (:character-literal "++") " cannot be used in a constant expression"))
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
           (throw-error -constant-error (:character-literal "--") " cannot be used in a constant expression"))
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
           (:local phase) " is " (:tag compile) ", only constant operations are permitted.")
    (define (plus (a object) (phase phase)) object
      (return (object-to-general-number a phase)))
    
    (%text :comment (:global-call minus a phase) " returns the value of the unary expression " (:character-literal "-") (:local a) ". If "
           (:local phase) " is " (:tag compile) ", only constant operations are permitted.")
    (define (minus (a object) (phase phase)) object
      (const x general-number (object-to-general-number a phase))
      (return (general-number-negate x)))
    
    (define (general-number-negate (x general-number)) general-number
      (case x
        (:narrow long (return (integer-to-long (neg (& value x)))))
        (:narrow u-long (return (integer-to-u-long (neg (& value x)))))
        (:narrow float32 (return (float32-negate x)))
        (:narrow float64 (return (float64-negate x)))))
    
    (define (bit-not (a object) (phase phase)) object
      (const x general-number (object-to-general-number a phase))
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
           (:local phase) " is " (:tag compile) ", only constant operations are permitted.")
    (define (logical-not (a object) (phase phase :unused)) object
      (return (not (object-to-boolean a))))
    
        
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
      (const x general-number (object-to-general-number a phase))
      (const y general-number (object-to-general-number b phase))
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
      (const x general-number (object-to-general-number a phase))
      (const y general-number (object-to-general-number b phase))
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
      (const x general-number (object-to-general-number a phase))
      (const y general-number (object-to-general-number b phase))
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
      (const ap primitive-object (object-to-primitive a none phase))
      (const bp primitive-object (object-to-primitive b none phase))
      (rwhen (or (in ap (union char16 string)) (in bp (union char16 string)))
        (return (append (object-to-string ap phase) (object-to-string bp phase))))
      (const x general-number (object-to-general-number ap phase))
      (const y general-number (object-to-general-number bp phase))
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
      (const x general-number (object-to-general-number a phase))
      (const y general-number (object-to-general-number b phase))
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
      (const x general-number (object-to-general-number a phase))
      (var count integer (truncate-to-integer (object-to-general-number b phase)))
      (case x
        (:narrow (union float32 float64)
          (<- count (bitwise-and count (hex #x1F)))
          (const i (integer-range (neg (expt 2 31)) (- (expt 2 31) 1)) (signed-wrap32 (bitwise-shift (truncate-to-integer x) count)))
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
      (const x general-number (object-to-general-number a phase))
      (var count integer (truncate-to-integer (object-to-general-number b phase)))
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
      (const x general-number (object-to-general-number a phase))
      (var count integer (truncate-to-integer (object-to-general-number b phase)))
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
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (const c class (object-to-class b))
         (return (is a c))))
      (production (:relational-expression :beta) ((:relational-expression :beta) as :shift-expression) relational-expression-as
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (const c class (object-to-class b))
         (return (coerce-or-null a c))))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression) relational-expression-in
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (const a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (return (has-property b a false phase))))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (var a object (read-reference ((eval :relational-expression) env phase) phase))
         (const b object (read-reference ((eval :shift-expression) env phase) phase))
         (cond
          ((in b class :narrow-true)
           (return (is a b)))
          ((is b -prototype-function)
           (const prototype object (dot-read b (list-set (new qualified-name public "prototype")) phase))
           (return (set-in prototype (archetypes a))))
          (nil 
           (throw-error -type-error))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    (define (is-less (a object) (b object) (phase phase)) boolean
      (const ap primitive-object (object-to-primitive a hint-number phase))
      (const bp primitive-object (object-to-primitive b hint-number phase))
      (rwhen (and (in ap (union char16 string) :narrow-true) (in bp (union char16 string) :narrow-true))
        (return (< (to-string ap) (to-string bp) string)))
      (return (= (general-number-compare (object-to-general-number ap phase) (object-to-general-number bp phase)) less order)))
    
    (define (is-less-or-equal (a object) (b object) (phase phase)) boolean
      (const ap primitive-object (object-to-primitive a hint-number phase))
      (const bp primitive-object (object-to-primitive b hint-number phase))
      (rwhen (and (in ap (union char16 string) :narrow-true) (in bp (union char16 string) :narrow-true))
        (return (<= (to-string ap) (to-string bp) string)))
      (return (in (general-number-compare (object-to-general-number ap phase) (object-to-general-number bp phase)) (tag less equal))))
    
    
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
            (return (is-equal (object-to-general-number a phase) b phase))))
        (:narrow general-number
          (const bp primitive-object (object-to-primitive b none phase))
          (case bp
            (:select (union undefined null) (return false))
            (:select (union boolean general-number char16 string) (return (= (general-number-compare a (object-to-general-number bp phase)) equal order)))))
        (:narrow (union char16 string)
          (const bp primitive-object (object-to-primitive b none phase))
          (case bp
            (:select (union undefined null) (return false))
            (:select (union boolean general-number) (return (= (general-number-compare (object-to-general-number a phase) (object-to-general-number bp phase)) equal order)))
            (:narrow (union char16 string) (return (= (to-string a) (to-string bp) string)))))
        (:select (union namespace compound-attribute class method-closure simple-instance date reg-exp package)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace compound-attribute class method-closure simple-instance date reg-exp package) (return (is-strictly-equal a b phase)))
            (:select (union boolean general-number char16 string)
              (const ap primitive-object (object-to-primitive a none phase))
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
      (const x general-number (object-to-general-number a phase))
      (const y general-number (object-to-general-number b phase))
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
      (const x general-number (object-to-general-number a phase))
      (const y general-number (object-to-general-number b phase))
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
      (const x general-number (object-to-general-number a phase))
      (const y general-number (object-to-general-number b phase))
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
         (if (object-to-boolean a)
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
         (const ba boolean (object-to-boolean a))
         (const bb boolean (object-to-boolean b))
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
         (if (object-to-boolean a)
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
         (if (object-to-boolean a)
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
         (if (object-to-boolean a)
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
           (throw-error -constant-error "assignment cannot be used in a constant expression"))
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
           (throw-error -constant-error "assignment cannot be used in a constant expression"))
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
           (throw-error -constant-error "assignment cannot be used in a constant expression"))
         (const r-left obj-or-ref ((eval :postfix-expression) env phase))
         (const o-left object (read-reference r-left phase))
         (const b-left boolean (object-to-boolean o-left))
         (var result object o-left)
         (case (operator :logical-assignment)
           (:select (tag and-eq)
             (when b-left
               (<- result (read-reference ((eval :assignment-expression) env phase) phase))))
           (:select (tag xor-eq)
             (const b-right boolean (object-to-boolean (read-reference ((eval :assignment-expression) env phase) phase)))
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
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Type Expressions")
    (rule (:type-expression :beta) ((validate (-> (context environment) void))
                                    (setup-and-eval (-> (environment) class)))
      (production (:type-expression :beta) ((:non-assignment-expression :beta)) type-expression-non-assignment-expression
        ((validate cxt env) :forward)
        ((setup-and-eval env)
         ((setup :non-assignment-expression))
         (const o object (read-reference ((eval :non-assignment-expression) env compile) compile))
         (return (object-to-class o)))))
    (%print-actions ("Validation" validate) ("Setup and Evaluation" setup-and-eval))
    
    
    (%heading 1 "Statements")
    
    (grammar-argument :omega
                      abbrev       ;optional semicolon when followed by a '}', 'else', or 'while' in a do-while
                      no-short-if  ;optional semicolon, but statement must not end with an if without an else
                      full)        ;semicolon required at the end
    (grammar-argument :omega_2 abbrev full)
    
    (rule (:statement :omega) ((validate (-> (context environment (list-set label) jump-targets boolean) void)) (setup (-> () void))
                               (eval (-> (environment object) object)))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        ((validate cxt env (sl :unused) (jt :unused) (preinst :unused)) ((validate :expression-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :expression-statement) env))))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((validate cxt env (sl :unused) (jt :unused) (preinst :unused)) ((validate :super-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :super-statement) env))))
      (production (:statement :omega) (:block) statement-block
        ((validate cxt env (sl :unused) jt preinst) ((validate :block) cxt env jt preinst))
        ((setup) :forward)
        ((eval env d) (return ((eval :block) env d))))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        ((validate cxt env sl jt (preinst :unused)) ((validate :labeled-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :labeled-statement) env d))))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        ((validate cxt env (sl :unused) jt (preinst :unused)) ((validate :if-statement) cxt env jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :if-statement) env d))))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((validate cxt env (sl :unused) jt (preinst :unused)) ((validate :switch-statement) cxt env jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :switch-statement) env d))))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((validate cxt env sl jt (preinst :unused)) ((validate :do-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :do-statement) env d))))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((validate cxt env sl jt (preinst :unused)) ((validate :while-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :while-statement) env d))))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((validate cxt env sl jt (preinst :unused)) ((validate :for-statement) cxt env sl jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :for-statement) env d))))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((validate cxt env (sl :unused) jt (preinst :unused)) ((validate :with-statement) cxt env jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :with-statement) env d))))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) jt (preinst :unused)) ((validate :continue-statement) jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :continue-statement) env d))))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        ((validate (cxt :unused) (env :unused) (sl :unused) jt (preinst :unused)) ((validate :break-statement) jt))
        ((setup) :forward)
        ((eval env d) (return ((eval :break-statement) env d))))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        ((validate cxt env (sl :unused) (jt :unused) (preinst :unused)) ((validate :return-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :return-statement) env))))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((validate cxt env (sl :unused) (jt :unused) (preinst :unused)) ((validate :throw-statement) cxt env))
        ((setup) :forward)
        ((eval env (d :unused)) (return ((eval :throw-statement) env))))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((validate cxt env (sl :unused) jt (preinst :unused)) ((validate :try-statement) cxt env jt))
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
        ((validate cxt env sl jt) ((validate :statement) cxt env sl jt false))
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
           (throw-error -type-error
             "attributes other than " (:character-literal "true") " and " (:character-literal "false")
             " may be used in a statement but not a substatement"))
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
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env)
         (return (read-reference ((eval :list-expression) env run) run)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Super Statement")
    (rule :super-statement ((validate (-> (context environment) void)) (setup (-> () void))
                            (eval (-> (environment) object)))
      (production :super-statement (super :arguments) super-statement-super-arguments
        ((validate cxt env)
         (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
         (rwhen (or (in frame (tag none) :narrow-false) (not-in (& kind frame) (tag constructor-function)))
           (throw-error -syntax-error "a " (:character-literal "super") " statement is meaningful only inside a constructor"))
         ((validate :arguments) cxt env)
         (&= calls-superconstructor frame true))
        ((setup) :forward)
        ((eval env)
         (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
         (assert (and (not-in frame (tag none) :narrow-true) (in (& kind frame) (tag constructor-function)))
           (:action validate) " already ensured that " (:assertion) ".")
         (const args (vector object) ((eval :arguments) env run))
         (rwhen (in (& superconstructor-called frame) (tag true))
           (throw-error -reference-error "the superconstructor cannot be called twice"))
         (const c class (assert-not-in (get-enclosing-class env) (tag none)))
         (const this object-opt (& this frame))
         (assert (in this simple-instance :narrow-true))
         (call-init this (& super c) args run)
         (&= superconstructor-called frame true)
         (return this))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Block Statement")
    (rule :block ((compile-frame (writable-cell local-frame))
                  (preinstantiate (writable-cell boolean))
                  (validate-using-frame (-> (context environment jump-targets boolean frame) void))
                  (validate (-> (context environment jump-targets boolean) void))
                  (setup (-> () void))
                  (eval (-> (environment object) object))
                  (eval-using-frame (-> (environment frame object) object)))
      (production :block ({ :directives }) block-directives
        ((validate-using-frame cxt env jt preinst frame)
         (const local-cxt context (new context (& strict cxt) (& open-namespaces cxt)))
         ((validate :directives) local-cxt (cons frame env) jt preinst none))
        ((validate cxt env jt preinst)
         (const compile-frame local-frame (new local-frame (list-set-of local-binding)))
         (action<- (compile-frame :block 0) compile-frame)
         (action<- (preinstantiate :block 0) preinst)
         ((validate-using-frame :block 0) cxt env jt preinst compile-frame))
        ((setup) :forward)
        ((eval env d)
         (const compile-frame local-frame (compile-frame :block 0))
         (var runtime-frame local-frame)
         (if (preinstantiate :block 0)
           (<- runtime-frame compile-frame)
           (<- runtime-frame (instantiate-local-frame compile-frame env)))
         (return ((eval :directives) (cons runtime-frame env) d)))
        ((eval-using-frame env frame d)
         (return ((eval :directives) (cons frame env) d)))))
    (%print-actions ("Validation" compile-frame preinstantiate validate validate-using-frame) ("Setup" setup) ("Evaluation" eval eval-using-frame))
    
    
    (%heading 2 "Labeled Statements")
    (rule (:labeled-statement :omega) ((validate (-> (context environment (list-set label) jump-targets) void)) (setup (-> () void))
                                       (eval (-> (environment object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((validate cxt env sl jt)
         (const name string (name :identifier))
         (rwhen (set-in name (& break-targets jt))
           (throw-error -syntax-error "nesting labeled statements with the same label is not permitted"))
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
         (if (object-to-boolean o)
           (return ((eval :substatement) env d))
           (return d))))
      (production (:if-statement full) (if :paren-list-expression (:substatement full)) if-statement-if-then-full
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label) jt))
        ((setup) :forward)
        ((eval env d)
         (const o object (read-reference ((eval :paren-list-expression) env run) run))
         (if (object-to-boolean o)
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
         (if (object-to-boolean o)
           (return ((eval :substatement 1) env d))
           (return ((eval :substatement 2) env d))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Switch Statement")
    (deftuple switch-key
      (key object))
    (deftype switch-guard (union switch-key (tag default) object))
    
    (rule :switch-statement ((compile-frame (writable-cell local-frame))
                             (validate (-> (context environment jump-targets) void))
                             (setup (-> () void))
                             (eval (-> (environment object) object)))
      (production :switch-statement (switch :paren-list-expression { :case-elements }) switch-statement-cases
        ((validate cxt env jt)
         (rwhen (> (n-defaults :case-elements) 1)
           (throw-error -syntax-error "a " (:character-literal "case") " statement may have at most one default clause"))
         ((validate :paren-list-expression) cxt env)
         (const jt2 jump-targets (new jump-targets
                                   (set+ (& break-targets jt) (list-set-of label default))
                                   (& continue-targets jt)))
         (const compile-frame local-frame (new local-frame (list-set-of local-binding)))
         (action<- (compile-frame :switch-statement 0) compile-frame)
         (const local-cxt context (new context (& strict cxt) (& open-namespaces cxt)))
         ((validate :case-elements) local-cxt (cons compile-frame env) jt2))
        ((setup) :forward)
        ((eval env d)
         (const key object (read-reference ((eval :paren-list-expression) env run) run))
         (const compile-frame local-frame (compile-frame :switch-statement 0))
         (const runtime-frame local-frame (instantiate-local-frame compile-frame env))
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
                          (validate (-> (context environment jump-targets) void))
                          (setup (-> () void))
                          (eval (-> (environment switch-guard object) switch-guard)))
      (production :case-elements () case-elements-none
        (n-defaults 0)
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval (env :unused) guard (d :unused)) (return guard)))
      (production :case-elements (:case-label) case-elements-one
        (n-defaults (n-defaults :case-label))
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval env guard d)
         (return ((eval :case-label) env guard d))))
      (production :case-elements (:case-label :case-elements-prefix (:case-element abbrev)) case-elements-more
        (n-defaults (+ (n-defaults :case-label) (+ (n-defaults :case-elements-prefix) (n-defaults :case-element))))
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval env guard d)
         (const guard2 switch-guard ((eval :case-label) env guard d))
         (const guard3 switch-guard ((eval :case-elements-prefix) env guard2 d))
         (return ((eval :case-element) env guard3 d)))))
    
    (rule :case-elements-prefix ((n-defaults integer)
                                 (validate (-> (context environment jump-targets) void))
                                 (setup (-> () void))
                                 (eval (-> (environment switch-guard object) switch-guard)))
      (production :case-elements-prefix () case-elements-prefix-none
        (n-defaults 0)
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval (env :unused) guard (d :unused)) (return guard)))
      (production :case-elements-prefix (:case-elements-prefix (:case-element full)) case-elements-prefix-more
        (n-defaults (+ (n-defaults :case-elements-prefix) (n-defaults :case-element)))
        ((validate cxt env jt) :forward)
        ((setup) :forward)
        ((eval env guard d)
         (const guard2 switch-guard ((eval :case-elements-prefix) env guard d))
         (return ((eval :case-element) env guard2 d)))))
    
    (rule (:case-element :omega_2) ((n-defaults integer)
                                    (validate (-> (context environment jump-targets) void))
                                    (setup (-> () void))
                                    (eval (-> (environment switch-guard object) switch-guard)))
      (production (:case-element :omega_2) ((:directive :omega_2)) case-element-directive
        (n-defaults 0)
        ((validate cxt env jt) ((validate :directive) cxt env jt false none))
        ((setup) :forward)
        ((eval env guard (d :unused))
         (case guard
           (:narrow (union switch-key (tag default)) (return guard))
           (:narrow object
             (return ((eval :directive) env guard))))))
      (production (:case-element :omega_2) (:case-label) case-element-case-label
        (n-defaults (n-defaults :case-label))
        ((validate cxt env jt) ((validate :case-label) cxt env jt))
        ((setup) :forward)
        ((eval env guard d)
         (return ((eval :case-label) env guard d)))))
    
    (rule :case-label ((n-defaults integer)
                       (validate (-> (context environment jump-targets) void))
                       (setup (-> () void))
                       (eval (-> (environment switch-guard object) switch-guard)))
      (production :case-label (case (:list-expression allow-in) \:) case-label-case
        (n-defaults 0)
        ((validate cxt env (jt :unused)) ((validate :list-expression) cxt env))
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
        ((validate (cxt :unused) (env :unused) (jt :unused)))
        ((setup) :forward)
        ((eval (env :unused) guard d)
         (case guard
           (:narrow (union switch-key object) (return guard))
           (:narrow (tag default) (return d))))))
    (%print-actions ("Validation" n-defaults compile-frame validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Do-While Statement")
    (rule :do-statement ((labels (writable-cell (list-set label)))
                         (validate (-> (context environment (list-set label) jump-targets) void))
                         (setup (-> () void))
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
                   (rwhen (not (object-to-boolean o))
                     (return d1))))
           (x) (if (and (in x break :narrow-true) (= (& label x) default label))
                 (return (& value x))
                 (throw x))))))
    (%print-actions ("Validation" labels validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "While Statement")
    (rule (:while-statement :omega) ((labels (writable-cell (list-set label)))
                                     (validate (-> (context environment (list-set label) jump-targets) void))
                                     (setup (-> () void))
                                     (eval (-> (environment object) object)))
      (production (:while-statement :omega) (while :paren-list-expression (:substatement :omega)) while-statement-while
        ((validate cxt env sl jt)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :while-statement 0) continue-labels)
         (const jt2 jump-targets (new jump-targets
                                   (set+ (& break-targets jt) (list-set-of label default))
                                   (set+ (& continue-targets jt) continue-labels)))
         ((validate :paren-list-expression) cxt env)
         ((validate :substatement) cxt env (list-set-of label) jt2))
        ((setup) :forward)
        ((eval env d)
         (catch ((var d1 object d)
                 (while (object-to-boolean (read-reference ((eval :paren-list-expression) env run) run))
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
    (rule (:for-statement :omega) ((labels (writable-cell (list-set label)))
                                   (compile-local-frame (writable-cell local-frame))
                                   (validate (-> (context environment (list-set label) jump-targets) void))
                                   (setup (-> () void))
                                   (eval (-> (environment object) object)))
      (production (:for-statement :omega) (for \( :for-initializer \; :optional-expression \; :optional-expression \)
                                               (:substatement :omega)) for-statement-c-style
        ((validate cxt env sl jt)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :for-statement 0) continue-labels)
         (const jt2 jump-targets (new jump-targets
                                   (set+ (& break-targets jt) (list-set-of label default))
                                   (set+ (& continue-targets jt) continue-labels)))
         (const compile-local-frame local-frame (new local-frame (list-set-of local-binding)))
         (action<- (compile-local-frame :for-statement 0) compile-local-frame)
         (const compile-env environment (cons compile-local-frame env))
         ((validate :for-initializer) cxt compile-env)
         ((validate :optional-expression 1) cxt compile-env)
         ((validate :optional-expression 2) cxt compile-env)
         ((validate :substatement) cxt compile-env (list-set-of label) jt2))
        ((setup) :forward)
        ((eval env d)
         (const runtime-local-frame local-frame (instantiate-local-frame (compile-local-frame :for-statement 0) env))
         (const runtime-env environment (cons runtime-local-frame env))
         (catch (((eval :for-initializer) runtime-env)
                 (var d1 object d)
                 (while (object-to-boolean (read-reference ((eval :optional-expression 1) runtime-env run) run))
                   (catch ((<- d1 ((eval :substatement) runtime-env d1)))
                     (x) (if (and (in x continue :narrow-true) (set-in (& label x) (labels :for-statement 0)))
                           (<- d1 (& value x))
                           (throw x)))
                   (exec (read-reference ((eval :optional-expression 2) runtime-env run) run)))
                 (return d1))
           (x) (if (and (in x break :narrow-true) (= (& label x) default label))
                 (return (& value x))
                 (throw x)))))
      
      (production (:for-statement :omega) (for \( :for-in-binding in (:list-expression allow-in) \) (:substatement :omega)) for-statement-in
        ((validate cxt env sl jt)
         (const continue-labels (list-set label) (set+ sl (list-set-of label default)))
         (action<- (labels :for-statement 0) continue-labels)
         (const jt2 jump-targets (new jump-targets
                                   (set+ (& break-targets jt) (list-set-of label default))
                                   (set+ (& continue-targets jt) continue-labels)))
         ((validate :list-expression) cxt env)
         (const compile-local-frame local-frame (new local-frame (list-set-of local-binding)))
         (action<- (compile-local-frame :for-statement 0) compile-local-frame)
         (const compile-env environment (cons compile-local-frame env))
         ((validate :for-in-binding) cxt compile-env)
         ((validate :substatement) cxt compile-env (list-set-of label) jt2))
        ((setup) :forward)
        ((eval env d)
         (catch ((const o object (read-reference ((eval :list-expression) env run) run))
                 (const c class (object-type o))
                 (var old-indices (list-set object) ((& enumerate c) o))
                 (var remaining-indices (list-set object) old-indices)
                 (var d1 object d)
                 (while (nonempty remaining-indices)
                   (const runtime-local-frame local-frame (instantiate-local-frame (compile-local-frame :for-statement 0) env))
                   (const runtime-env environment (cons runtime-local-frame env))
                   (const index object (elt-of remaining-indices))
                   (<- remaining-indices (set- remaining-indices (list-set index)))
                   ((write-binding :for-in-binding) runtime-env index)
                   (catch ((<- d1 ((eval :substatement) runtime-env d1)))
                     (x) (if (and (in x continue :narrow-true) (set-in (& label x) (labels :for-statement 0)))
                           (<- d1 (& value x))
                           (throw x)))
                   (var new-indices (list-set object) ((& enumerate c) o))
                   (when (/= new-indices old-indices (list-set object))
                     (// "The implementation may, at its discretion, add none, some, or all of the objects in the set difference "
                         (:expr (list-set object) (set- new-indices old-indices)) " to " (:local remaining-indices) ";")
                     (// "The implementation may, at its discretion, remove none, some, or all of the objects in the set difference "
                         (:expr (list-set object) (set- old-indices new-indices)) " from " (:local remaining-indices) ";"))
                   (<- old-indices new-indices))
                 (return d1))
           (x) (if (and (in x break :narrow-true) (= (& label x) default label))
                 (return (& value x))
                 (throw x))))))
    
    
    (rule :for-initializer ((enabled (writable-cell boolean))
                            (validate (-> (context environment) void))
                            (setup (-> () void))
                            (eval (-> (environment) void)))
      (production :for-initializer () for-initializer-empty
        ((validate (cxt :unused) (env :unused)))
        ((setup))
        ((eval (env :unused))))
      (production :for-initializer ((:list-expression no-in)) for-initializer-expression
        ((validate cxt env) ((validate :list-expression) cxt env))
        ((setup) ((setup :list-expression)))
        ((eval env) (exec (read-reference ((eval :list-expression) env run) run))))
      (production :for-initializer ((:variable-definition no-in)) for-initializer-variable-definition
        ((validate cxt env) ((validate :variable-definition) cxt env none))
        ((setup) ((setup :variable-definition)))
        ((eval env) (exec ((eval :variable-definition) env undefined))))
      (production :for-initializer (:attributes :no-line-break (:variable-definition no-in)) for-initializer-attribute-variable-definition
        ((validate cxt env)
         ((validate :attributes) cxt env)
         ((setup :attributes))
         (const attr attribute ((eval :attributes) env compile))
         (action<- (enabled :for-initializer 0) (not-in attr false-type))
         (when (not-in attr false-type :narrow-true)
           ((validate :variable-definition) cxt env attr)))
        ((setup)
         (when (enabled :for-initializer 0)
           ((setup :variable-definition))))
        ((eval env)
         (when (enabled :for-initializer 0)
           (exec ((eval :variable-definition) env undefined))))))
    
    
    (rule :for-in-binding ((validate (-> (context environment) void))
                           (setup (-> () void))
                           (write-binding (-> (environment object) void)))
      (production :for-in-binding (:postfix-expression) for-in-binding-expression
        ((validate cxt env) ((validate :postfix-expression) cxt env))
        ((setup) ((setup :postfix-expression)))
        ((write-binding env new-value)
         (const r obj-or-ref ((eval :postfix-expression) env run))
         (exec (write-reference r new-value run))))
      (production :for-in-binding (:variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition
        ((validate cxt env) ((validate :variable-binding) cxt env none (immutable :variable-definition-kind) true))
        ((setup) ((setup :variable-binding)))
        ((write-binding env new-value) ((write-binding :variable-binding) env new-value)))
      (production :for-in-binding (:attributes :no-line-break :variable-definition-kind (:variable-binding no-in)) for-in-binding-attribute-variable-definition
        ((validate cxt env)
         ((validate :attributes) cxt env)
         ((setup :attributes))
         (const attr attribute ((eval :attributes) env compile))
         (rwhen (in attr false-type :narrow-false)
           (throw-error -attribute-error
             "the " (:character-literal "false") " attribute canot be applied to a " (:character-literal "for") "-" (:character-literal "in") " variable definition"))
         ((validate :variable-binding) cxt env attr (immutable :variable-definition-kind) true))
        ((setup) ((setup :variable-binding)))
        ((write-binding env new-value) ((write-binding :variable-binding) env new-value))))
    
    
    (rule :optional-expression ((validate (-> (context environment) void)) (setup (-> () void))
                                (eval (-> (environment phase) obj-or-ref)))
      (production :optional-expression ((:list-expression allow-in)) optional-expression-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :list-expression) env phase))))
      (production :optional-expression () optional-expression-empty
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return true))))
    (%print-actions ("Validation" enabled labels compile-local-frame validate) ("Setup" setup) ("Evaluation" eval write-binding))
    
    
    (%heading 2 "With Statement")
    (rule (:with-statement :omega) ((compile-local-frame (writable-cell local-frame))
                                    (validate (-> (context environment jump-targets) void)) (setup (-> () void))
                                    (eval (-> (environment object) object)))
      (production (:with-statement :omega) (with :paren-list-expression (:substatement :omega)) with-statement-with
        ((validate cxt env jt)
         ((validate :paren-list-expression) cxt env)
         (const compile-with-frame with-frame (new with-frame none))
         (const compile-local-frame local-frame (new local-frame (list-set-of local-binding)))
         (action<- (compile-local-frame :with-statement 0) compile-local-frame)
         (const compile-env environment (cons compile-local-frame (cons compile-with-frame env)))
         ((validate :substatement) cxt compile-env (list-set-of label) jt))
        ((setup) :forward)
        ((eval env d)
         (const value object (read-reference ((eval :paren-list-expression) env run) run))
         (const runtime-with-frame with-frame (new with-frame value))
         (const runtime-local-frame local-frame (instantiate-local-frame (compile-local-frame :with-statement 0) (cons runtime-with-frame env)))
         (const runtime-env environment (cons runtime-local-frame (cons runtime-with-frame env)))
         (return ((eval :substatement) runtime-env d)))))
    (%print-actions ("Validation" compile-local-frame validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Continue and Break Statements")
    (rule :continue-statement ((validate (-> (jump-targets) void)) (setup (-> () void))
                               (eval (-> (environment object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((validate jt)
         (rwhen (set-not-in default (& continue-targets jt))
           (throw-error -syntax-error "there is no enclosing statement to which to continue")))
        ((setup))
        ((eval (env :unused) d) (throw (new continue d default))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((validate jt)
         (rwhen (set-not-in (name :identifier) (& continue-targets jt))
           (throw-error -syntax-error "there is no enclosing labeled statement to which to continue")))
        ((setup))
        ((eval (env :unused) d) (throw (new continue d (name :identifier))))))
    
    (rule :break-statement ((validate (-> (jump-targets) void)) (setup (-> () void))
                            (eval (-> (environment object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((validate jt)
         (rwhen (set-not-in default (& break-targets jt))
           (throw-error -syntax-error "there is no enclosing statement to which to break")))
        ((setup))
        ((eval (env :unused) d) (throw (new break d default))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((validate jt)
         (rwhen (set-not-in (name :identifier) (& break-targets jt))
           (throw-error -syntax-error "there is no enclosing labeled statement to which to break")))
        ((setup))
        ((eval (env :unused) d) (throw (new break d (name :identifier))))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Return Statement")
    (rule :return-statement ((validate (-> (context environment) void)) (setup (-> () void))
                             (eval (-> (environment) object)))
      (production :return-statement (return) return-statement-default
        ((validate (cxt :unused) env)
         (rwhen (in (get-enclosing-parameter-frame env) (tag none))
           (throw-error -syntax-error "a " (:character-literal "return") " statement must be located inside a function")))
        ((setup) :forward)
        ((eval (env :unused)) (throw (new return undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((validate cxt env)
         (const frame parameter-frame-opt (get-enclosing-parameter-frame env))
         (rwhen (in frame (tag none) :narrow-false)
           (throw-error -syntax-error "a " (:character-literal "return") " statement must be located inside a function"))
         (rwhen (cannot-return-value frame)
           (throw-error -syntax-error "a " (:character-literal "return") " statement inside a setter or constructor cannot return a value"))
         ((validate :list-expression) cxt env))
        ((setup) :forward)
        ((eval env)
         (const a object (read-reference ((eval :list-expression) env run) run))
         (throw (new return a)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    (%text :comment (:global-call cannot-return-value frame) " returns " (:tag true) " if the function represented by " (:local frame)
           " cannot return a value because it is a setter or constructor.")
    (define (cannot-return-value (frame parameter-frame)) boolean
      (return (or (in (& kind frame) (tag constructor-function)) (in (& handling frame) (tag set)))))
    
    
    (%heading 2 "Throw Statement")
    (rule :throw-statement ((validate (-> (context environment) void)) (setup (-> () void))
                            (eval (-> (environment) object)))
      (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env)
         (const a object (read-reference ((eval :list-expression) env run) run))
         (throw a))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Try Statement")
    (rule :try-statement ((validate (-> (context environment jump-targets) void)) (setup (-> () void))
                          (eval (-> (environment object) object)))
      (production :try-statement (try :block :catch-clauses) try-statement-catch-clauses
        ((validate cxt env jt)
         ((validate :block) cxt env jt false)
         ((validate :catch-clauses) cxt env jt))
        ((setup) :forward)
        ((eval env d)
         (catch ((return ((eval :block) env d)))
           (x)
           (cond
            ((in x control-transfer :narrow-false) (throw x))
            (nil 
             (const r (union object (tag reject)) ((eval :catch-clauses) env x))
             (if (not-in r (tag reject) :narrow-true)
               (return r)
               (throw x)))))))
      
      (production :try-statement (try :block :catch-clauses-opt finally :block) try-statement-catch-clauses-finally
        ((validate cxt env jt)
         ((validate :block 1) cxt env jt false)
         ((validate :catch-clauses-opt) cxt env jt)
         ((validate :block 2) cxt env jt false))
        ((setup) :forward)
        ((eval env d)
         (var result object-opt none)
         (var exception (union semantic-exception (tag none)) none)
         (catch ((<- result ((eval :block 1) env d)))
           (x) (<- exception x))
         (assert (xor (in result (tag none)) (in exception (tag none))) "At this point exactly one of " (:local result) " and " (:local exception) " has a non-" (:tag none) " value.")
         (when (in exception object)
           (catch ((const r (union object (tag reject)) ((eval :catch-clauses-opt) env (assert-in exception object)))
                   (when (not-in r (tag reject) :narrow-true)
                     (note "The exception has been handled, so clear it.")
                     (<- result r)
                     (<- exception none)))
             (x)
             (note "The " (:character-literal "catch") " clause threw another exception or " (:type control-transfer) " " (:local x) ", so replace the original exception with " (:local x) ".")
             (<- exception x)))
         (note "The " (:character-literal "finally") " clause is executed even if the original block exited due to a " (:type control-transfer) " ("
               (:character-literal "break") ", " (:character-literal "continue") ", or " (:character-literal "return") ").")
         (note "The " (:character-literal "finally") " clause is not inside a " (:keyword try) "-" (:keyword catch)
               " semantic statement, so if it throws another exception or " (:type control-transfer) ", then the original exception or " (:type control-transfer) " "
               (:local exception) " is dropped.")
         (exec ((eval :block 2) env undefined))
         (assert (xor (in result (tag none)) (in exception (tag none))) "At this point exactly one of " (:local result) " and " (:local exception) " has a non-" (:tag none) " value.")
         (if (not-in exception (tag none) :narrow-true)
           (throw exception)
           (return (assert-not-in result (tag none)))))))
    
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
    
    (rule :catch-clause ((compile-env (writable-cell environment))
                         (compile-frame (writable-cell local-frame))
                         (validate (-> (context environment jump-targets) void)) (setup (-> () void))
                         (eval (-> (environment object) (union object (tag reject)))))
      (production :catch-clause (catch \( :parameter \) :block) catch-clause-block
        ((validate cxt env jt)
         (const compile-frame local-frame (new local-frame (list-set-of local-binding)))
         (const compile-env environment (cons compile-frame env))
         (action<- (compile-frame :catch-clause 0) compile-frame)
         (action<- (compile-env :catch-clause 0) compile-env)
         ((validate :parameter) cxt compile-env compile-frame)
         ((validate :block) cxt compile-env jt false))
        ((setup)
         ((setup :parameter) (compile-env :catch-clause 0) (compile-frame :catch-clause 0) none)
         ((setup :block)))
        ((eval env exception)
         (const compile-frame local-frame (compile-frame :catch-clause 0))
         (const runtime-frame local-frame (instantiate-local-frame compile-frame env))
         (const runtime-env environment (cons runtime-frame env))
         (const qname qualified-name (new qualified-name public (name :parameter)))
         (const v singleton-property-opt (find-local-singleton-property runtime-frame (list-set qname) write))
         (assert (in v variable :narrow-true) (:action validate) " created one local variable with the name in " (:local qname) ", so " (:assertion) ".")
         (cond
          ((is exception (&opt type v))
           (write-singleton-property v exception run)
           (return ((eval :block) runtime-env undefined)))
          (nil (return reject))))))
    (%print-actions ("Validation" compile-env compile-frame validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 1 "Directives")
    (rule (:directive :omega_2) ((enabled (writable-cell boolean))
                                 (validate (-> (context environment jump-targets boolean attribute-opt-not-false) void))
                                 (setup (-> () void))
                                 (eval (-> (environment object) object)))
      (production (:directive :omega_2) (:empty-statement) directive-empty-statement
        ((validate (cxt :unused) (env :unused) (jt :unused) (preinst :unused) (attr :unused)))
        ((setup))
        ((eval (env :unused) d) (return d)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        ((validate cxt env jt preinst attr)
         (rwhen (not-in attr (tag none true))
           (throw-error -attribute-error "an ordinary statement only permits the attributes " (:character-literal "true") " and " (:character-literal "false")))
         ((validate :statement) cxt env (list-set-of label) jt preinst))
        ((setup) ((setup :statement)))
        ((eval env d) (return ((eval :statement) env d))))
      (production (:directive :omega_2) ((:annotatable-directive :omega_2)) directive-annotatable-directive
        ((validate cxt env (jt :unused) preinst attr) ((validate :annotatable-directive) cxt env preinst attr))
        ((setup) ((setup :annotatable-directive)))
        ((eval env d) (return ((eval :annotatable-directive) env d))))
      (production (:directive :omega_2) (:attributes :no-line-break (:annotatable-directive :omega_2)) directive-attributes-and-directive
        ((validate cxt env (jt :unused) preinst attr)
         ((validate :attributes) cxt env)
         ((setup :attributes))
         (const attr2 attribute ((eval :attributes) env compile))
         (const attr3 attribute (combine-attributes attr attr2))
         (cond
          ((in attr3 false-type :narrow-false)
           (action<- (enabled :directive 0) false))
          (nil
           (action<- (enabled :directive 0) true)
           ((validate :annotatable-directive) cxt env preinst attr3))))
        ((setup)
         (when (enabled :directive 0)
           ((setup :annotatable-directive))))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :annotatable-directive) env d))
           (return d))))
      (production (:directive :omega_2) (:attributes :no-line-break { :directives }) directive-annotated-group
        ((validate cxt env jt preinst attr)
         ((validate :attributes) cxt env)
         ((setup :attributes))
         (const attr2 attribute ((eval :attributes) env compile))
         (const attr3 attribute (combine-attributes attr attr2))
         (cond
          ((in attr3 false-type :narrow-false)
           (action<- (enabled :directive 0) false))
          (nil
           (action<- (enabled :directive 0) true)
           (const local-cxt context (new context (& strict cxt) (& open-namespaces cxt)))
           ((validate :directives) local-cxt env jt preinst attr3))))
        ((setup)
         (when (enabled :directive 0)
           ((setup :directives))))
        ((eval env d)
         (if (enabled :directive 0)
           (return ((eval :directives) env d))
           (return d))))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((validate (cxt :unused) (env :unused) (jt :unused) (preinst :unused) attr)
           (if (in attr (tag none true))
             (todo)
             (throw-error -attribute-error "an " (:character-literal "include") " directive only permits the attributes " (:character-literal "true") " and " (:character-literal "false"))))
          ((setup) (todo))
          ((eval (env :unused) (d :unused)) (todo))))
      (production (:directive :omega_2) (:pragma (:semicolon :omega_2)) directive-pragma
        ((validate cxt (env :unused) (jt :unused) (preinst :unused) attr)
         (if (in attr (tag none true))
           ((validate :pragma) cxt)
           (throw-error -attribute-error "a " (:character-literal "pragma") " directive only permits the attributes " (:character-literal "true") " and " (:character-literal "false"))))
        ((setup))
        ((eval (env :unused) d) (return d))))
    
    (rule (:annotatable-directive :omega_2) ((validate (-> (context environment boolean attribute-opt-not-false) void))
                                             (setup (-> () void))
                                             (eval (-> (environment object) object)))
      (production (:annotatable-directive :omega_2) ((:variable-definition allow-in) (:semicolon :omega_2)) annotatable-directive-variable-definition
        ((validate cxt env (preinst :unused) attr) ((validate :variable-definition) cxt env attr))
        ((setup) ((setup :variable-definition)))
        ((eval env d) (return ((eval :variable-definition) env d))))
      (production (:annotatable-directive :omega_2) (:function-definition) annotatable-directive-function-definition
        ((validate cxt env preinst attr) ((validate :function-definition) cxt env preinst attr))
        ((setup) ((setup :function-definition)))
        ((eval (env :unused) d) (return d)))
      (production (:annotatable-directive :omega_2) (:class-definition) annotatable-directive-class-definition
        ((validate cxt env preinst attr) ((validate :class-definition) cxt env preinst attr))
        ((setup) ((setup :class-definition)))
        ((eval env d) (return ((eval :class-definition) env d))))
      (production (:annotatable-directive :omega_2) (:namespace-definition (:semicolon :omega_2)) annotatable-directive-namespace-definition
        ((validate cxt env preinst attr) ((validate :namespace-definition) cxt env preinst attr))
        ((setup))
        ((eval (env :unused) d) (return d)))
      ;(production (:annotatable-directive :omega_2) ((:interface-definition :omega_2)) annotatable-directive-interface-definition
      ;  ((validate (cxt :unused) (env :unused) (preinst :unused) (attr :unused)) (todo))
      ;  ((setup) (todo))
      ;  ((eval (env :unused) (d :unused)) (todo)))
      (production (:annotatable-directive :omega_2) (:import-directive (:semicolon :omega_2)) annotatable-directive-import-directive
        ((validate cxt env preinst attr) ((validate :import-directive) cxt env preinst attr))
        ((setup))
        ((eval (env :unused) d) (return d)))
      (? js2
        (production (:annotatable-directive :omega_2) (:export-definition (:semicolon :omega_2)) annotatable-directive-export-definition
          ((validate (cxt :unused) (env :unused) (preinst :unused) (attr :unused)) (todo))
          ((setup) (todo))
          ((eval (env :unused) (d :unused)) (todo))))
      (production (:annotatable-directive :omega_2) (:use-directive (:semicolon :omega_2)) annotatable-directive-use-directive
        ((validate cxt env (preinst :unused) attr)
         (if (in attr (tag none true))
           ((validate :use-directive) cxt env)
           (throw-error -attribute-error
             "a " (:character-literal "use") " directive only permits the attributes " (:character-literal "true") " and " (:character-literal "false"))))
        ((setup))
        ((eval (env :unused) d) (return d))))
    
    
    (rule :directives ((validate (-> (context environment jump-targets boolean attribute-opt-not-false) void)) (setup (-> () void))
                       (eval (-> (environment object) object)))
      (production :directives () directives-none
        ((validate cxt env jt preinst attr) :forward)
        ((setup) :forward)
        ((eval (env :unused) d) (return d)))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((validate cxt env jt preinst attr) :forward)
        ((setup) :forward)
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    
    (rule :directives-prefix ((validate (-> (context environment jump-targets boolean attribute-opt-not-false) void)) (setup (-> () void))
                              (eval (-> (environment object) object)))
      (production :directives-prefix () directives-prefix-none
        ((validate cxt env jt preinst attr) :forward)
        ((setup) :forward)
        ((eval (env :unused) d) (return d)))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((validate cxt env jt preinst attr) :forward)
        ((setup) :forward)
        ((eval env d)
         (const o object ((eval :directives-prefix) env d))
         (return ((eval :directive) env o)))))
    (%print-actions ("Validation" enabled validate) ("Setup" setup) ("Evaluation" eval))
    
    
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
         (return (object-to-attribute a phase))))
      (production :attribute (true) attribute-true
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return true)))
      (production :attribute (false) attribute-false
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval (env :unused) (phase :unused)) (return false)))
      (production :attribute (:reserved-namespace) attribute-reserved-namespace
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase) (return ((eval :reserved-namespace) env phase)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    
    (%heading 2 "Use Directive")
    (rule :use-directive ((validate (-> (context environment) void)))
      (production :use-directive (use namespace :paren-list-expression) use-directive-normal
        ((validate cxt env)
         ((validate :paren-list-expression) cxt env)
         ((setup :paren-list-expression))
         (const values (vector object) ((eval-as-list :paren-list-expression) env compile))
         (var namespaces (list-set namespace) (list-set-of namespace))
         (for-each values v
           (rwhen (not-in v namespace :narrow-false)
             (throw-error -type-error))
           (<- namespaces (set+ namespaces (list-set v))))
         (&= open-namespaces cxt (set+ (& open-namespaces cxt) namespaces)))))
    (%print-actions ("Validation" validate))
    
    
    (%heading 2 "Import Directive")
    (rule :import-directive ((validate (-> (context environment boolean attribute-opt-not-false) void)))
      (production :import-directive (import :package-name) import-directive-unnamed
        ((validate (cxt :unused) env preinst attr)
         (rwhen (not preinst)
           (throw-error -syntax-error "a package may be imported only in a preinstantiated scope"))
         (const frame frame (nth env 0))
         (rwhen (not-in frame package :narrow-false)
           (throw-error -syntax-error "a package may be imported only into a package scope"))
         (rwhen (not-in attr (tag none true))
           (throw-error -attribute-error
             "an unnamed " (:character-literal "import") " directive only permits the attributes " (:character-literal "true") " and " (:character-literal "false")))
         (const pkg-name string (name :package-name))
         (const pkg package (locate-package pkg-name))
         (import-package-into pkg frame)))
      (production :import-directive (import :identifier = :package-name) import-directive-named
        ((validate (cxt :unused) env preinst attr)
         (rwhen (not preinst)
           (throw-error -syntax-error "a package may be imported only in a preinstantiated scope"))
         (const frame frame (nth env 0))
         (rwhen (not-in frame package :narrow-false)
           (throw-error -syntax-error "a package may be imported only into a package scope"))
         (const a compound-attribute (to-compound-attribute attr))
         (rwhen (& dynamic a)
           (throw-error -attribute-error "a package definition cannot have the " (:character-literal "dynamic") " attribute"))
         (rwhen (& prototype a)
           (throw-error -attribute-error "a package definition cannot have the " (:character-literal "prototype") " attribute"))
         (const pkg-name string (name :package-name))
         (const pkg package (locate-package pkg-name))
         (const v variable (new variable -package pkg true none none :uninit))
         (exec (define-singleton-property env (name :identifier) (& namespaces a) (& override-mod a) (& explicit a) read-write v))
         (import-package-into pkg frame))))
    (%print-actions ("Validation" validate))
    
    
    (define (locate-package (name string)) package
      (/* "Look for a package bound to " (:local name) " in the implementation" :apostrophe "s list of available packages. "
          "If one is found, let " (:local pkg) ":" :nbsp (:type package) " be that package; otherwise, throw an implementation-defined error.")
      (var pkg (union package (tag none)) none)
      (reserve pkg2)
      (when (some package-database pkg2 (= (& name pkg2) name string) :define-true)
        (<- pkg pkg2))
      (rwhen (in pkg (tag none) :narrow-false)
        (throw-error -error "package not found"))
      (*/)
      (const initialize (union (-> () void) (tag none busy)) (& initialize pkg))
      (case initialize
        (:select (tag none))
        (:select (tag busy) (throw-error -uninitialized-error "circular package dependency"))
        (:narrow (-> () void)
          (initialize)
          (assert (in (& initialize pkg) (tag none)))))
      (return pkg))
    
    
    (define (import-package-into (source package) (destination package)) void
      (for-each (& local-bindings source) b
        (when (not (or (& explicit b)
                       (in (& content b) (tag forbidden))
                       (some (& local-bindings destination) d (and (= (& qname b) (& qname d) qualified-name) (accesses-overlap (& accesses b) (& accesses d))))))
          (&= local-bindings destination (set+ (& local-bindings destination) (list-set b))))))
    
    
      #|
      (%heading 2 "Import Directive")
      (production :import-directive (import :import-binding :includes-excludes) import-directive-import)
      (production :import-directive (import :import-binding \, namespace :paren-list-expression :includes-excludes)
                  import-directive-import-namespaces)
      
      (production :import-binding (:package-name) import-binding-import-source)
      (production :import-binding (:identifier = :package-name) import-binding-named-import-source)
      
      
      (production :includes-excludes () includes-excludes-none)
      (production :includes-excludes (\, exclude \( :name-patterns \)) includes-excludes-exclude-list)
      (production :includes-excludes (\, include \( :name-patterns \)) includes-excludes-include-list)
      
      (production :name-patterns () name-patterns-empty)
      (production :name-patterns (:name-pattern-list) name-patterns-name-pattern-list)
      
      (production :name-pattern-list (:qualified-identifier) name-pattern-list-one)
      (production :name-pattern-list (:name-pattern-list \, :qualified-identifier) name-pattern-list-more)
      
      
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
    (rule :pragma ((validate (-> (context) void)))
      (production :pragma (use :pragma-items) pragma-pragma-items
        ((validate cxt) :forward)))
    
    (rule :pragma-items ((validate (-> (context) void)))
      (production :pragma-items (:pragma-item) pragma-items-one
        ((validate cxt) :forward))
      (production :pragma-items (:pragma-items \, :pragma-item) pragma-items-more
        ((validate cxt) :forward)))
    
    (rule :pragma-item ((validate (-> (context) void)))
      (production :pragma-item (:pragma-expr) pragma-item-pragma-expr
        ((validate cxt) ((validate :pragma-expr) cxt false)))
      (production :pragma-item (:pragma-expr \?) pragma-item-optional-pragma-expr
        ((validate cxt) ((validate :pragma-expr) cxt true))))
    
    (rule :pragma-expr ((validate (-> (context boolean) void)))
      (production :pragma-expr (:identifier) pragma-expr-identifier
        ((validate cxt optional)
         (process-pragma cxt (name :identifier) undefined optional)))
      (production :pragma-expr (:identifier \( :pragma-argument \)) pragma-expr-identifier-and-parameter
        ((validate cxt optional)
         (const arg object (value :pragma-argument))
         (process-pragma cxt (name :identifier) arg optional))))
    
    (rule :pragma-argument ((value object))
      (production :pragma-argument (true) pragma-argument-true (value true))
      (production :pragma-argument (false) pragma-argument-false (value false))
      (production :pragma-argument ($number) pragma-argument-number (value (value $number)))
      (production :pragma-argument (- $number) pragma-argument-negative-number (value (general-number-negate (value $number))))
      (production :pragma-argument (- $negated-min-long) pragma-argument-min-long (value (new long (neg (expt 2 63)))))
      (production :pragma-argument ($string) pragma-argument-string (value (value $string))))
    (%print-actions ("Validation" validate))
    
    (define (process-pragma (cxt context) (name string) (value object) (optional boolean)) void
      (when (= name "strict" string)
        (rwhen (in value (tag true undefined) :narrow-false)
          (&= strict cxt true)
          (return))
        (rwhen (in value (tag false))
          (&= strict cxt false)
          (return)))
      (when (= name "ecmascript" string)
        (rwhen (set-in value (list-set-of object undefined 4.0))
          (return))
        (rwhen (set-in value (list-set-of object 1.0 2.0 3.0))
          (// "An implementation may optionally modify " (:local cxt) " to disable features not available in ECMAScript Edition " (:local value)
              " other than subsequent pragmas.")
          (return)))
      (rwhen (not optional)
        (throw-error -syntax-error)))
    
    
    (%heading 1 "Definitions")
    (? js2
      (%heading 2 "Export Definition")
      (production :export-definition (export :export-binding-list) export-definition-definition)
      
      (production :export-binding-list (:export-binding) export-binding-list-one)
      (production :export-binding-list (:export-binding-list \, :export-binding) export-binding-list-more)
      
      (production :export-binding (:function-name) export-binding-simple)
      (production :export-binding (:function-name = :function-name) export-binding-initializer))
    
    
    (%heading 2 "Variable Definition")
    (rule (:variable-definition :beta) ((validate (-> (context environment attribute-opt-not-false) void)) (setup (-> () void))
                                        (eval (-> (environment object) object)))
      (production (:variable-definition :beta) (:variable-definition-kind (:variable-binding-list :beta)) variable-definition-definition
        ((validate cxt env attr)
         ((validate :variable-binding-list) cxt env attr (immutable :variable-definition-kind) false))
        ((setup) ((setup :variable-binding-list)))
        ((eval env d)
         ((eval :variable-binding-list) env)
         (return d))))
    
    (rule :variable-definition-kind ((immutable boolean))
      (production :variable-definition-kind (var) variable-definition-kind-var (immutable false))
      (production :variable-definition-kind (const) variable-definition-kind-const (immutable true)))
    
    (rule (:variable-binding-list :beta) ((validate (-> (context environment attribute-opt-not-false boolean boolean) void))
                                          (setup (-> () void))
                                          (eval (-> (environment) void)))
      (production (:variable-binding-list :beta) ((:variable-binding :beta)) variable-binding-list-one
        ((validate cxt env attr immutable no-initializer) :forward)
        ((setup) :forward)
        ((eval env) :forward))
      (production (:variable-binding-list :beta) ((:variable-binding-list :beta) \, (:variable-binding :beta)) variable-binding-list-more
        ((validate cxt env attr immutable no-initializer) :forward)
        ((setup) :forward)
        ((eval env) :forward)))
    
    
    (rule (:variable-binding :beta) ((compile-env (writable-cell environment))
                                     (compile-var (writable-cell (union variable dynamic-var instance-variable)))
                                     (overridden-var (writable-cell instance-variable-opt))
                                     (multiname (writable-cell multiname))
                                     (validate (-> (context environment attribute-opt-not-false boolean boolean) void))
                                     (setup (-> () void))
                                     (eval (-> (environment) void))
                                     (write-binding (-> (environment object) void)))
      (production (:variable-binding :beta) ((:typed-identifier :beta) (:variable-initialisation :beta)) variable-binding-full
        ((validate cxt env attr immutable no-initializer)
         ((validate :typed-identifier) cxt env)
         ((validate :variable-initialisation) cxt env)
         (action<- (compile-env :variable-binding 0) env)
         (const name string (name :typed-identifier))
         (cond
          ((and (not (& strict cxt)) (in (get-regional-frame env) (union package parameter-frame))
                (not immutable) (in attr (tag none)) (plain :typed-identifier))
           (const qname qualified-name (new qualified-name public name))
           (action<- (multiname :variable-binding 0) (list-set qname))
           (action<- (compile-var :variable-binding 0) (define-hoisted-var env name undefined)))
          (nil
           (const a compound-attribute (to-compound-attribute attr))
           (rwhen (& dynamic a)
             (throw-error -attribute-error "a variable definition cannot have the " (:character-literal "dynamic") " attribute"))
           (rwhen (& prototype a)
             (throw-error -attribute-error "a variable definition cannot have the " (:character-literal "prototype") " attribute"))
           (var category property-category (& category a))
           (if (in (nth env 0) class)
             (when (in category (tag none))
               (<- category final))
             (rwhen (not-in category (tag none))
               (throw-error -attribute-error
                 "non-class variables cannot have a " (:character-literal "static") ", " (:character-literal "virtual") ", or "
                 (:character-literal "final") " attribute")))
           (case category
             (:select (tag none static)
               (const initializer initializer-opt (initializer :variable-initialisation))
               (rwhen (and no-initializer (not-in initializer (tag none)))
                 (throw-error -syntax-error
                   "a " (:character-literal "for") "-" (:character-literal "in") " statement" :apostrophe "s variable definition must not have an initialiser"))
               (function (variable-setup) class-opt
                 (const type class-opt ((setup-and-eval :typed-identifier) env))
                 ((setup :variable-initialisation))
                 (return type))
               (const v variable (new variable :uninit none immutable variable-setup initializer env))
               (const multiname multiname (define-singleton-property env name (& namespaces a) (& override-mod a) (& explicit a) read-write v))
               (action<- (multiname :variable-binding 0) multiname)
               (action<- (compile-var :variable-binding 0) v))
             (:narrow (tag virtual final)
               (assert (not no-initializer))
               (const c class (assert-in (nth env 0) class))
               (const v instance-variable (new instance-variable :uninit (in category (tag final)) :uninit :uninit :uninit immutable))
               (const v-overridden instance-variable-opt (assert-in (define-instance-property c cxt name (& namespaces a) (& override-mod a) (& explicit a) v)
                                                                    instance-variable-opt))
               (var enumerable boolean (& enumerable a))
               (when (and (not-in v-overridden (tag none) :narrow-true) (&opt enumerable v-overridden))
                 (<- enumerable true))
               (&const= enumerable v enumerable)
               (action<- (overridden-var :variable-binding 0) v-overridden)
               (action<- (compile-var :variable-binding 0) v))))))
        
        ((setup)
         (const env environment (compile-env :variable-binding 0))
         (const v (union variable dynamic-var instance-variable) (compile-var :variable-binding 0))
         (case v
           (:narrow variable
             (setup-variable v)
             (when (not (& immutable v))
               (const default-value object-opt (& default-value (&opt type v)))
               (rwhen (in default-value (tag none) :narrow-false)
                 (throw-error -uninitialized-error "Cannot declare a mutable variable of type " (:character-literal "Never")))
               (&= value v default-value)))
           (:select dynamic-var
             ((setup :variable-initialisation)))
           (:narrow instance-variable
             (var t class-opt ((setup-and-eval :typed-identifier) env))
             (when (in t (tag none))
               (const overridden-var instance-variable-opt (overridden-var :variable-binding 0))
               (if (not-in overridden-var (tag none) :narrow-true)
                 (<- t (&opt type overridden-var))
                 (<- t -object)))
             (quiet-assert (not-in t (tag none) :narrow-true))
             (&const= type v t)
             ((setup :variable-initialisation))
             (const initializer initializer-opt (initializer :variable-initialisation))
             (var default-value object-opt none)
             (cond
              ((not-in initializer (tag none) :narrow-true)
               (<- default-value (initializer env compile)))
              ((not (& immutable v))
               (<- default-value (& default-value t))
               (rwhen (in default-value (tag none))
                 (throw-error -uninitialized-error "Cannot declare a mutable instance variable of type " (:character-literal "Never")))))
             (&const= default-value v default-value))))
        
        ((eval env)
         (case (compile-var :variable-binding 0)
           (:select variable
             (const inner-frame non-with-frame (assert-not-in (nth env 0) with-frame))
             (const properties (list-set singleton-property) (map (& local-bindings inner-frame) b (& content b) (set-in (& qname b) (multiname :variable-binding 0))))
             (note "The " (:local properties) " set consists of exactly one " (:type variable) " element because " (:local inner-frame)
                   " was constructed with that " (:type variable) " inside " (:action validate) ".")
             (const v variable (assert-in (unique-elt-of properties) variable))
             (const initializer (union initializer (tag none busy)) (& initializer v))
             (case initializer
               (:select (tag none))
               (:select (tag busy) (throw-error -reference-error))
               (:narrow initializer
                 (&= initializer v busy)
                 (const value object (initializer (&opt initializer-env v) run))
                 (exec (write-variable v value true)))))
           (:select dynamic-var
             (const initializer initializer-opt (initializer :variable-initialisation))
             (when (not-in initializer (tag none) :narrow-true)
               (const value object (initializer env run))
               (lexical-write env (multiname :variable-binding 0) value false run)))
           (:select instance-variable)))
        
        ((write-binding env new-value)
         (case (assert-not-in (compile-var :variable-binding 0) instance-variable)
           (:select variable
             (const inner-frame non-with-frame (assert-not-in (nth env 0) with-frame))
             (const properties (list-set singleton-property) (map (& local-bindings inner-frame) b (& content b) (set-in (& qname b) (multiname :variable-binding 0))))
             (note "The " (:local properties) " set consists of exactly one " (:type variable) " element because " (:local inner-frame)
                   " was constructed with that " (:type variable) " inside " (:action validate) ".")
             (const v variable (assert-in (unique-elt-of properties) variable))
             (exec (write-variable v new-value false)))
           (:select dynamic-var
             (lexical-write env (multiname :variable-binding 0) new-value false run))))))
    
    
    (rule (:variable-initialisation :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                            (initializer initializer-opt))
      (production (:variable-initialisation :beta) () variable-initialisation-none
        ((validate cxt env) :forward)
        ((setup) :forward)
        (initializer none))
      (production (:variable-initialisation :beta) (= (:variable-initializer :beta)) variable-initialisation-variable-initializer
        ((validate cxt env) :forward)
        ((setup) :forward)
        (initializer (eval :variable-initializer))))
    
    (rule (:variable-initializer :beta) ((validate (-> (context environment) void)) (setup (-> () void))
                                         (eval (-> (environment phase) object)))
      (production (:variable-initializer :beta) ((:assignment-expression :beta)) variable-initializer-assignment-expression
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env phase)
         (return (read-reference ((eval :assignment-expression) env phase) phase))))
      (production (:variable-initializer :beta) (:attribute-combination) variable-initializer-attribute-combination
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
                    ("Evaluation" setup-and-eval eval write-binding initializer))
    
    
    (%heading 2 "Simple Variable Definition")
    (%text :syntax "A " (:grammar-symbol :simple-variable-definition) " represents the subset of " (:grammar-symbol :variable-definition)
           " expansions that may be used when the variable definition is used as a " (:grammar-symbol (:substatement :omega))
           " instead of a " (:grammar-symbol (:directive :omega_2)) " in non-strict mode. "
           "In strict mode variable definitions may not be used as substatements.")
    (rule :simple-variable-definition ((validate (-> (context environment) void)) (setup (-> () void))
                                       (eval (-> (environment object) object)))
      (production :simple-variable-definition (var :untyped-variable-binding-list) simple-variable-definition-definition
        ((validate cxt env)
         (rwhen (or (& strict cxt) (not-in (get-regional-frame env) (union package parameter-frame)))
           (throw-error -syntax-error
             "a variable may not be defined in a substatement except inside a non-strict function or non-strict top-level code; "
             "to fix this error, place the definition inside a block"))
         ((validate :untyped-variable-binding-list) cxt env))
        ((setup) :forward)
        ((eval env d)
         ((eval :untyped-variable-binding-list) env)
         (return d))))
    
    (rule :untyped-variable-binding-list ((validate (-> (context environment) void)) (setup (-> () void))
                                          (eval (-> (environment) void)))
      (production :untyped-variable-binding-list (:untyped-variable-binding) untyped-variable-binding-list-one
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env) :forward))
      (production :untyped-variable-binding-list (:untyped-variable-binding-list \, :untyped-variable-binding) untyped-variable-binding-list-more
        ((validate cxt env) :forward)
        ((setup) :forward)
        ((eval env) :forward)))
    
    (rule :untyped-variable-binding ((validate (-> (context environment) void)) (setup (-> () void))
                                     (eval (-> (environment) void)))
      (production :untyped-variable-binding (:identifier (:variable-initialisation allow-in)) untyped-variable-binding-full
        ((validate cxt env)
         ((validate :variable-initialisation) cxt env)
         (exec (define-hoisted-var env (name :identifier) undefined)))
        ((setup) ((setup :variable-initialisation)))
        ((eval env)
         (const initializer initializer-opt (initializer :variable-initialisation))
         (when (not-in initializer (tag none) :narrow-true)
           (const value object (initializer env run))
           (const qname qualified-name (new qualified-name public (name :identifier)))
           (lexical-write env (list-set qname) value false run)))))
    (%print-actions ("Validation" validate) ("Setup" setup) ("Evaluation" eval))
    
    
    (%heading 2 "Function Definition")
    (rule :function-definition ((overridden-property (writable-cell instance-property-opt))
                                (validate-static (-> (context environment boolean compound-attribute boolean boolean) void))
                                (validate-instance (-> (context environment class compound-attribute boolean) void))
                                (validate-constructor (-> (context environment class compound-attribute) void))
                                (validate (-> (context environment boolean attribute-opt-not-false) void))
                                (setup (-> () void)))
      (production :function-definition (function :function-name :function-common) function-definition-definition
        ((validate-static cxt env preinst a unchecked hoisted)
         (const name string (name :function-name))
         (const handling handling (handling :function-name))
         (case handling
           (:select (tag normal)
             (var kind static-function-kind)
             (cond
              (unchecked (<- kind unchecked-function))
              ((& prototype a) (<- kind prototype-function))
              (nil (<- kind plain-function)))
             (var f (union simple-instance uninstantiated-function) ((validate-static-function :function-common) cxt env kind))
             (when preinst
               (<- f (instantiate-function (assert-in f uninstantiated-function) env)))
             (cond
              (hoisted
               (exec (define-hoisted-var env name f)))
              (nil
               (const v variable (new variable -function f true none none :uninit))
               (exec (define-singleton-property env name (& namespaces a) (& override-mod a) (& explicit a) read-write v)))))
           (:narrow (tag get set)
             (rwhen (& prototype a)
               (throw-error -attribute-error "a getter or setter cannot have the " (:character-literal "prototype") " attribute"))
             (assert (not (or unchecked hoisted)))
             ((validate :function-common) cxt env plain-function handling)
             (var bound-env environment-opt none)
             (when preinst
               (<- bound-env env))
             (case handling
               (:select (tag get)
                 (const getter getter (new getter (eval-static-get :function-common) bound-env))
                 (exec (define-singleton-property env name (& namespaces a) (& override-mod a) (& explicit a) read getter)))
               (:select (tag set)
                 (const setter setter (new setter (eval-static-set :function-common) bound-env))
                 (exec (define-singleton-property env name (& namespaces a) (& override-mod a) (& explicit a) write setter))))))
         (action<- (overridden-property :function-definition 0) none))
        
        ((validate-instance cxt env c a final)
         (rwhen (& prototype a)
           (throw-error -attribute-error "an instance method cannot have the " (:character-literal "prototype") " attribute"))
         (const handling handling (handling :function-name))
         ((validate :function-common) cxt env instance-function handling)
         (const signature parameter-frame (compile-frame :function-common))
         (var m instance-property)
         (case handling
           (:select (tag normal)
             (<- m (new instance-method :uninit final :uninit signature (signature-length signature) (eval-instance-call :function-common))))
           (:select (tag get)
             (<- m (new instance-getter :uninit final :uninit signature (eval-instance-get :function-common))))
           (:select (tag set)
             (<- m (new instance-setter :uninit final :uninit signature (eval-instance-set :function-common)))))
         (const m-overridden instance-property-opt (define-instance-property c cxt (name :function-name) (& namespaces a) (& override-mod a) (& explicit a) m))
         (var enumerable boolean (& enumerable a))
         (when (and (not-in m-overridden (tag none) :narrow-true) (&opt enumerable m-overridden))
           (<- enumerable true))
         (&const= enumerable m enumerable)
         (action<- (overridden-property :function-definition 0) m-overridden))
        
        ((validate-constructor cxt env c a)
         (rwhen (& prototype a)
           (throw-error -attribute-error "a class constructor cannot have the " (:character-literal "prototype") " attribute"))
         (rwhen (in (handling :function-name) (tag get set))
           (throw-error -syntax-error "a class constructor cannot be a getter or a setter"))
         ((validate :function-common) cxt env constructor-function normal)
         (rwhen (not-in (& init c) (tag none))
           (throw-error -definition-error "duplicate constructor definition"))
         (&= init c (eval-instance-init :function-common))
         (action<- (overridden-property :function-definition 0) none))
        
        ((validate cxt env preinst attr)
         (const a compound-attribute (to-compound-attribute attr))
         (rwhen (& dynamic a)
           (throw-error -attribute-error "a function cannot have the " (:character-literal "dynamic") " attribute"))
         (const frame frame (nth env 0))
         (cond
          ((in frame class :narrow-true)
           (assert preinst)
           (case (& category a)
             (:select (tag static)
               ((validate-static :function-definition 0) cxt env preinst a false false))
             (:select (tag none)
               (if (= (name :function-name) (& name frame) string)
                 ((validate-constructor :function-definition 0) cxt env frame a)
                 ((validate-instance :function-definition 0) cxt env frame a false)))
             (:select (tag virtual)
               ((validate-instance :function-definition 0) cxt env frame a false))
             (:select (tag final)
               ((validate-instance :function-definition 0) cxt env frame a true))))
          (nil
           (rwhen (not-in (& category a) (tag none))
             (throw-error -attribute-error
               "non-class functions cannot have a " (:character-literal "static") ", " (:character-literal "virtual") ", or "
               (:character-literal "final") " attribute"))
           (const unchecked boolean (and (not (& strict cxt)) (in (handling :function-name) (tag normal)) (plain :function-common)))
           (const hoisted boolean (and unchecked
                                       (in attr (tag none))
                                       (or (in frame package) (and (in frame local-frame) (in (nth env 1) parameter-frame)))))
           ((validate-static :function-definition 0) cxt env preinst a unchecked hoisted))))
        
        ((setup)
         (const overridden-property instance-property-opt (overridden-property :function-definition 0))
         (case overridden-property
           (:select (tag none)
             ((setup :function-common)))
           (:narrow (union instance-method instance-getter instance-setter)
             ((setup-override :function-common) (&opt signature overridden-property)))
           (:narrow instance-variable
             (var overridden-signature parameter-frame)
             (case (handling :function-name)
               (:select (tag normal)
                 (bottom "This cannot happen because " (:action validate-instance) " already ensured that a function cannot override an instance variable."))
               (:select (tag get)
                 (<- overridden-signature (new parameter-frame (list-set-of local-binding) instance-function get false false none
                                               (vector-of parameter) none (&opt type overridden-property))))
               (:select (tag set)
                 (const v variable (new variable (&opt type overridden-property) none false none none :uninit))
                 (const parameters (vector parameter) (vector (new parameter v none)))
                 (<- overridden-signature (new parameter-frame (list-set-of local-binding) instance-function set false false none
                                               parameters none -void))))
             ((setup-override :function-common) overridden-signature))))))
    
    
    (rule :function-name ((handling handling) (name string))
      (production :function-name (:identifier) function-name-function
        (handling normal)
        (name (name :identifier)))
      (production :function-name (get :no-line-break :identifier) function-name-getter
        (handling get)
        (name (name :identifier)))
      (production :function-name (set :no-line-break :identifier) function-name-setter
        (handling set)
        (name (name :identifier))))
    
    
    (rule :function-common ((plain boolean)
                            (compile-env (writable-cell environment))
                            (compile-frame (writable-cell parameter-frame))
                            (validate (-> (context environment function-kind handling) void))
                            (setup (-> () void))
                            (setup-override (-> (parameter-frame) void))
                            (eval-static-call (-> (object simple-instance (vector object) phase) object))
                            (eval-static-get (-> (environment phase) object))
                            (eval-static-set (-> (object environment phase) void))
                            (eval-instance-call (-> (object (vector object) phase) object))
                            (eval-instance-get (-> (object phase) object))
                            (eval-instance-set (-> (object object phase) void))
                            (eval-instance-init (-> (simple-instance (vector object) (tag run)) void))
                            (eval-prototype-construct (-> (simple-instance (vector object) phase) object))
                            (validate-static-function (-> (context environment static-function-kind) uninstantiated-function)))
      (production :function-common (\( :parameters \) :result :block) function-common-signatures-and-block
        (plain (and (plain :parameters) (plain :result)))
        ((validate cxt env kind handling)
         (const local-cxt context (new context (& strict cxt) (& open-namespaces cxt)))
         (const superconstructor-called boolean (not-in kind (tag constructor-function)))
         (const compile-frame parameter-frame (new parameter-frame (list-set-of local-binding) kind handling false superconstructor-called none
                                                   (vector-of parameter) none :uninit))
         (const compile-env environment (cons compile-frame env))
         (action<- (compile-frame :function-common 0) compile-frame)
         (action<- (compile-env :function-common 0) compile-env)
         (when (in kind (tag unchecked-function))
           (exec (define-hoisted-var compile-env "arguments" undefined)))
         ((validate :parameters) local-cxt compile-env compile-frame)
         ((validate :result) local-cxt compile-env)
         ((validate :block) local-cxt compile-env (new jump-targets (list-set-of label) (list-set-of label)) false))
        
        ((setup)
         (const compile-env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         ((setup :parameters) compile-env compile-frame)
         (check-accessor-parameters compile-frame)
         ((setup :result) compile-env compile-frame)
         ((setup :block)))
        
        ((setup-override overridden-signature)
         (const compile-env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         ((setup-override :parameters) compile-env compile-frame overridden-signature)
         (check-accessor-parameters compile-frame)
         ((setup-override :result) compile-env compile-frame overridden-signature)
         ((setup :block)))
        
        ((eval-static-call this f args phase)
         (note "The check that " (:expr boolean (not-in phase (tag compile))) " also ensures that " (:action setup) " has been called.")
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "a constant expression cannot call user-defined functions"))
         (const runtime-env environment (assert-not-in (& env f) (tag none)))
         (var runtime-this object-opt none)
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (when (in (& kind compile-frame) (tag unchecked-function prototype-function))
           (if (in this primitive-object)
             (<- runtime-this (get-package-frame runtime-env))
             (<- runtime-this this)))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame runtime-env runtime-this))
         (assign-arguments runtime-frame f args phase)
         (var result object)
         (catch ((exec ((eval :block) (cons runtime-frame runtime-env) undefined))
                 (<- result undefined))
           (x) (if (in x return :narrow-true)
                 (<- result (& value x))
                 (throw x)))
         (return (coerce result (&opt return-type runtime-frame))))
        
        ((eval-static-get runtime-env phase)
         (note "The check that " (:expr boolean (not-in phase (tag compile))) " also ensures that " (:action setup) " has been called.")
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "a constant expression cannot call user-defined getters"))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame runtime-env none))
         (assign-arguments runtime-frame none (vector-of object) phase)
         (var result object)
         (catch ((exec ((eval :block) (cons runtime-frame runtime-env) undefined))
                 (throw-error -syntax-error "a getter must return a value and may not return by falling off the end of its code"))
           (x) (if (in x return :narrow-true)
                 (<- result (& value x))
                 (throw x)))
         (return (coerce result (&opt return-type runtime-frame))))
        
        ((eval-static-set new-value runtime-env phase)
         (note "The check that " (:expr boolean (not-in phase (tag compile))) " also ensures that " (:action setup) " has been called.")
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "a constant expression cannot call setters"))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame runtime-env none))
         (assign-arguments runtime-frame none (vector new-value) phase)
         (catch ((exec ((eval :block) (cons runtime-frame runtime-env) undefined)))
           (x) (rwhen (not-in x return)
                 (throw x))))
        
        ((eval-instance-call this args phase)
         (note "The check that " (:expr boolean (not-in phase (tag compile))) " also ensures that " (:action setup) " has been called.")
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "a constant expression cannot call user-defined functions"))
         (note "Class frames are always preinstantiated, so the run environment is the same as compile environment.")
         (const env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame env this))
         (assign-arguments runtime-frame none args phase)
         (var result object)
         (catch ((exec ((eval :block) (cons runtime-frame env) undefined))
                 (<- result undefined))
           (x) (if (in x return :narrow-true)
                 (<- result (& value x))
                 (throw x)))
         (return (coerce result (&opt return-type runtime-frame))))
        
        ((eval-instance-get this phase)
         (note "The check that " (:expr boolean (not-in phase (tag compile))) " also ensures that " (:action setup) " has been called.")
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "a constant expression cannot call user-defined getters"))
         (note "Class frames are always preinstantiated, so the run environment is the same as compile environment.")
         (const env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame env this))
         (assign-arguments runtime-frame none (vector-of object) phase)
         (var result object)
         (catch ((exec ((eval :block) (cons runtime-frame env) undefined))
                 (throw-error -syntax-error "a getter must return a value and may not return by falling off the end of its code"))
           (x) (if (in x return :narrow-true)
                 (<- result (& value x))
                 (throw x)))
         (return (coerce result (&opt return-type runtime-frame))))
        
        ((eval-instance-set this new-value phase)
         (note "The check that " (:expr boolean (not-in phase (tag compile))) " also ensures that " (:action setup) " has been called.")
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "a constant expression cannot call setters"))
         (note "Class frames are always preinstantiated, so the run environment is the same as compile environment.")
         (const env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame env this))
         (assign-arguments runtime-frame none (vector new-value) phase)
         (catch ((exec ((eval :block) (cons runtime-frame env) undefined)))
           (x) (rwhen (not-in x return)
                 (throw x))))
        
        ((eval-instance-init this args phase)
         (note "Class frames are always preinstantiated, so the run environment is the same as compile environment.")
         (const env environment (compile-env :function-common 0))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame env this))
         (assign-arguments runtime-frame none args phase)
         (when (not (& calls-superconstructor runtime-frame))
           (const c class (assert-not-in (get-enclosing-class env) (tag none)))
           (call-init this (& super c) (vector-of object) run)
           (&= superconstructor-called runtime-frame true))
         (catch ((exec ((eval :block) (cons runtime-frame env) undefined)))
           (x) (rwhen (not-in x return)
                 (throw x)))
         (rwhen (not (& superconstructor-called runtime-frame))
           (throw-error -uninitialized-error "the superconstructor must be called before returning normally from a constructor")))
        
        ((eval-prototype-construct f args phase)
         (note "The check that " (:expr boolean (not-in phase (tag compile))) " also ensures that " (:action setup) " has been called.")
         (rwhen (in phase (tag compile) :narrow-false)
           (throw-error -constant-error "a constant expression cannot call user-defined prototype constructors"))
         (const runtime-env environment (assert-not-in (& env f) (tag none)))
         (var archetype object (dot-read f (list-set (new qualified-name public "prototype")) phase))
         (cond
          ((in archetype (tag null undefined)) (<- archetype -object-prototype))
          ((/= (object-type archetype) -object class) (throw-error -type-error "bad " (:character-literal "prototype") " value")))
         (var o object (create-simple-instance -object archetype none none none))
         (const compile-frame parameter-frame (compile-frame :function-common 0))
         (const runtime-frame parameter-frame (instantiate-parameter-frame compile-frame runtime-env o))
         (assign-arguments runtime-frame f args phase)
         (var result object)
         (catch ((exec ((eval :block) (cons runtime-frame runtime-env) undefined))
                 (<- result undefined))
           (x) (if (in x return :narrow-true)
                 (<- result (& value x))
                 (throw x)))
         (const coerced-result object (coerce result (&opt return-type runtime-frame)))
         (if (in coerced-result primitive-object)
           (return o)
           (return coerced-result)))
        
        ((validate-static-function cxt env kind)
         ((validate :function-common 0) cxt env kind normal)
         (const length integer (parameter-count :parameters))
         (case kind
           (:select (tag plain-function)
             (return (new uninstantiated-function -function length (eval-static-call :function-common 0) none (list-set-of simple-instance))))
           (:select (tag unchecked-function prototype-function)
             (return (new uninstantiated-function -prototype-function length (eval-static-call :function-common 0) (eval-prototype-construct :function-common 0)
                          (list-set-of simple-instance))))))))

    (%print-actions ("Validation" overridden-property handling name plain compile-env compile-frame
                     validate validate-static validate-instance validate-constructor validate-static-function)
                    ("Setup" setup setup-override)
                    ("Evaluation" eval-static-call eval-static-get eval-static-set eval-instance-call eval-instance-get eval-instance-set eval-instance-init eval-prototype-construct))
    
    
    (define (check-accessor-parameters (frame parameter-frame)) void
      (const parameters (vector parameter) (&opt parameters frame))
      (const rest variable-opt (&opt rest frame))
      (case (& handling frame)
        (:select (tag normal))
        (:select (tag get)
          (rwhen (or (nonempty parameters) (not-in rest (tag none)))
            (throw-error -syntax-error "a getter cannot take any parameters")))
        (:select (tag set)
          (rwhen (or (/= (length parameters) 1) (not-in rest (tag none)))
            (throw-error -syntax-error "a setter must take exactly one parameter"))
          (rwhen (not-in (& default (nth parameters 0)) (tag none))
            (throw-error -syntax-error "a setter" :apostrophe "s parameter cannot be optional")))))
    
    
    (define (assign-arguments (runtime-frame parameter-frame) (f (union simple-instance (tag none))) (args (vector object)) (phase (tag run))) void
      (// "This procedure performs a number of checks on the arguments, including checking their count, names, and values. "
          "Although this procedure performs these checks in a specific order for expository purposes, an implementation may perform these checks in a different "
          "order, which could have the effect of reporting a different error if there are multiple errors. "
          "For example, if a function only allows between 2 and 4 arguments, the first of which must be a " (:character-literal "Number")
          " and is passed five arguments the first of which is a " (:character-literal "String") ", then the implementation may throw an exception either about "
          "the argument count mismatch or about the type coercion error in the first argument.")
      (var arguments-object object-opt none)
      (when (in (& kind runtime-frame) (tag unchecked-function))
        (<- arguments-object (construct -array (vector-of object) phase))
        (create-dynamic-property (assert-in arguments-object simple-instance) (new qualified-name public "callee") false false (assert-not-in f (tag none)))
        (write-array-private-length (assert-not-in arguments-object (tag none)) (length args) phase))
      (var rest-object object-opt none)
      (const rest (union variable (tag none)) (&opt rest runtime-frame))
      (when (not-in rest (tag none) :narrow-true)
        (<- rest-object (construct -array (vector-of object) phase)))
      (const parameters (vector parameter) (&opt parameters runtime-frame))
      (var i integer 0)
      (var j integer 0)
      (for-each args arg
        (cond
         ((< i (length parameters))
          (const parameter parameter (nth parameters i))
          (var default object-opt (& default parameter))
          (var arg-or-default object arg)
          (when (and (in arg-or-default (tag undefined)) (not-in default (tag none) :narrow-true))
            (<- arg-or-default default))
          (const v (union dynamic-var variable) (& var parameter))
          (write-singleton-property v arg-or-default phase)
          (when (not-in arguments-object (tag none) :narrow-true)
            (note "Create an alias of " (:local v) " as the " (:local i) "th entry of the " (:character-literal "arguments") " object.")
            (assert (in v dynamic-var))
            (const qname qualified-name (object-to-qualified-name (real-to-float64 i) phase))
            (&= local-bindings (assert-in arguments-object simple-instance) (set+ (& local-bindings (assert-in arguments-object simple-instance))
                                                                                  (list-set (new local-binding qname read-write false false v))))))
         ((not-in rest-object (tag none) :narrow-true)
          (rwhen (>= j array-limit)
            (throw-error -range-error))
          (index-write rest-object j arg phase)
          (assert (in arguments-object (tag none))
            (:assertion) " because a function can't have both a rest parameter and an " (:character-literal "arguments") " object.")
          (<- j (+ j 1)))
         ((not-in arguments-object (tag none) :narrow-true)
          (index-write arguments-object i arg phase))
         (nil (throw-error -argument-error
                "more arguments than parameters were supplied, and the called function does not have a " (:character-literal "...") " parameter and is not unchecked.")))
        (<- i (+ i 1)))
      (while (< i (length parameters))
        (const parameter parameter (nth parameters i))
        (var default object-opt (& default parameter))
        (when (in default (tag none))
          (if (not-in arguments-object (tag none))
            (<- default undefined)
            (throw-error -argument-error
              "fewer arguments than parameters were supplied, and the called function does not supply default values for the missing parameters and is not unchecked.")))
        (quiet-assert (not-in default (tag none) :narrow-true))
        (write-singleton-property (& var parameter) default phase)
        (<- i (+ i 1))))
    
    
    (define (signature-length (signature parameter-frame)) integer
      (return (length (&opt parameters signature))))
    
    
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
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature"))))
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
        ((setup compile-env compile-frame)
         ((setup :parameter-init) compile-env compile-frame))
        ((setup-override compile-env compile-frame overridden-signature overridden-parameters)
         (rwhen (empty overridden-parameters)
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature"))
         ((setup-override :parameter-init) compile-env compile-frame (nth overridden-parameters 0))
         (rwhen (or (/= (length overridden-parameters) 1)
                    (not-in (&opt rest overridden-signature) (tag none)))
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature"))))
      (production :nonempty-parameters (:parameter-init \, :nonempty-parameters) nonempty-parameters-parameter-init-and-more
        (plain (and (plain :parameter-init) (plain :nonempty-parameters)))
        (parameter-count (+ 1 (parameter-count :nonempty-parameters)))
        ((validate cxt env compile-frame) :forward)
        ((setup compile-env compile-frame)
         ((setup :parameter-init) compile-env compile-frame)
         ((setup :nonempty-parameters) compile-env compile-frame))
        ((setup-override compile-env compile-frame overridden-signature overridden-parameters)
         (rwhen (empty overridden-parameters)
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature"))
         ((setup-override :parameter-init) compile-env compile-frame (nth overridden-parameters 0))
         ((setup-override :nonempty-parameters) compile-env compile-frame overridden-signature (subseq overridden-parameters 1))))
      (production :nonempty-parameters (:rest-parameter) nonempty-parameters-rest-parameter
        (plain false)
        (parameter-count 0)
        ((validate cxt env compile-frame) :forward)
        ((setup (compile-env :unused) (compile-frame :unused)))
        ((setup-override (compile-env :unused) (compile-frame :unused) overridden-signature overridden-parameters)
         (rwhen (nonempty overridden-parameters)
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature"))
         (const overridden-rest (union variable (tag none)) (&opt rest overridden-signature))
         (rwhen (or (in overridden-rest (tag none) :narrow-false) (/= (&opt type overridden-rest) -array class))
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature")))))
    
    
    (rule :parameter ((name string)
                      (plain boolean)
                      (compile-var (writable-cell (union dynamic-var variable)))
                      (validate (-> (context environment (union parameter-frame local-frame)) void))
                      (setup (-> (environment (union parameter-frame local-frame) object-opt) void))
                      (setup-override (-> (environment parameter-frame object-opt parameter) void)))
      (production :parameter (:parameter-attributes (:typed-identifier allow-in)) parameter-attributes-and-typed-identifier
        (name (name :typed-identifier))
        (plain (and (plain :typed-identifier) (not (has-const :parameter-attributes))))
        ((validate cxt env compile-frame)
         ((validate :typed-identifier) cxt env)
         (const immutable boolean (has-const :parameter-attributes))
         (const name string (name :typed-identifier))
         (var v (union dynamic-var variable))
         (cond
          ((and (in compile-frame parameter-frame :narrow-true) (in (& kind compile-frame) (tag unchecked-function)))
           (assert (not immutable))
           (<- v (define-hoisted-var env name undefined)))
          (nil
           (<- v (new variable :uninit none immutable none none :uninit))
           (exec (define-singleton-property env name (list-set public) none false read-write v))))
         (action<- (compile-var :parameter 0) v))
        ((setup compile-env compile-frame default)
         (rwhen (and (in compile-frame parameter-frame :narrow-true)
                     (in default (tag none))
                     (some (&opt parameters compile-frame) p2 (not-in (& default p2) (tag none))))
           (throw-error -syntax-error "a required parameter cannot follow an optional one"))
         (const v (union dynamic-var variable) (compile-var :parameter 0))
         (case v
           (:select dynamic-var)
           (:narrow variable
             (var type class-opt ((setup-and-eval :typed-identifier) compile-env))
             (when (in type (tag none))
               (<- type -object))
             (quiet-assert (not-in type (tag none) :narrow-true))
             (&const= type v type)))
         (when (in compile-frame parameter-frame :narrow-true)
           (const p parameter (new parameter v default))
           (&= parameters compile-frame (append (&opt parameters compile-frame) (vector p)))))
        ((setup-override compile-env compile-frame default overridden-parameter)
         (var new-default object-opt default)
         (when (in new-default (tag none))
           (<- new-default (& default overridden-parameter)))
         (rwhen (and (in default (tag none)) (some (&opt parameters compile-frame) p2 (not-in (& default p2) (tag none))))
           (throw-error -syntax-error "a required parameter cannot follow an optional one"))
         (const v (union dynamic-var variable) (compile-var :parameter 0))
         (assert (not-in v dynamic-var :narrow-true))
         (var type class-opt ((setup-and-eval :typed-identifier) compile-env))
         (when (in type (tag none))
           (<- type -object))
         (quiet-assert (not-in type (tag none) :narrow-true))
         (rwhen (/= type (&opt type (assert-not-in (& var overridden-parameter) dynamic-var)) class)
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature"))
         (&const= type v type)
         (const p parameter (new parameter v new-default))
         (&= parameters compile-frame (append (&opt parameters compile-frame) (vector p))))))
    
    
    (rule :parameter-attributes ((has-const boolean))
      (production :parameter-attributes () parameter-parameter-none
        (has-const false))
      (production :parameter-attributes (const) parameter-parameter-const
        (has-const true)))
    
    
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
      (production :parameter-init (:parameter = (:assignment-expression allow-in)) parameter-init-initializer
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
    
    
    (rule :rest-parameter ((validate (-> (context environment parameter-frame) void)))
      (production :rest-parameter (\.\.\.) rest-parameter-none
        ((validate (cxt :unused) (env :unused) compile-frame)
         (assert (not-in (& kind compile-frame) (tag unchecked-function)))
         (const v variable (new variable -array none true none none :uninit))
         (&= rest compile-frame v)))
      (production :rest-parameter (\.\.\. :parameter-attributes :identifier) rest-parameter-parameter
        ((validate (cxt :unused) env compile-frame)
         (assert (not-in (& kind compile-frame) (tag unchecked-function)))
         (const v variable (new variable -array none (has-const :parameter-attributes) none none :uninit))
         (&= rest compile-frame v)
         (const name string (name :identifier))
         (exec (define-singleton-property env name (list-set public) none false read-write v)))))
    
    
    (rule :result ((plain boolean)
                   (validate (-> (context environment) void))
                   (setup (-> (environment parameter-frame) void))
                   (setup-override (-> (environment parameter-frame parameter-frame) void)))
      (production :result () result-none
        (plain true)
        ((validate cxt env) :forward)
        ((setup (compile-env :unused) compile-frame)
         (var default-return-type class -object)
         (when (cannot-return-value compile-frame)
           (<- default-return-type -void))
         (&const= return-type compile-frame default-return-type))
        ((setup-override (compile-env :unused) compile-frame overridden-signature)
         (&const= return-type compile-frame (&opt return-type overridden-signature))))
      (production :result (\: (:type-expression allow-in)) result-colon-and-type-expression
        (plain false)
        ((validate cxt env) :forward)
        ((setup compile-env compile-frame)
         (rwhen (cannot-return-value compile-frame)
           (throw-error -syntax-error "a setter or constructor cannot define a return type"))
         (&const= return-type compile-frame ((setup-and-eval :type-expression) compile-env)))
        ((setup-override compile-env compile-frame overridden-signature)
         (const t class ((setup-and-eval :type-expression) compile-env))
         (rwhen (/= (&opt return-type overridden-signature) t class)
           (throw-error -definition-error "mismatch with the overridden method" :apostrophe "s signature"))
         (&const= return-type compile-frame t)))
      ;(production :result ((:- {) (:type-expression allow-in)) result-type-expression)
      )
    (%print-actions ("Validation" name plain has-const parameter-count compile-var validate) ("Setup" setup setup-override))
    
    
    (%heading 2 "Class Definition")
    (rule :class-definition ((class (writable-cell class))
                             (validate (-> (context environment boolean attribute-opt-not-false) void))
                             (setup (-> () void))
                             (eval (-> (environment object) object)))
      (production :class-definition (class :identifier :inheritance :block) class-definition-definition
        ((validate cxt env preinst attr)
         (rwhen (not preinst)
           (throw-error -syntax-error "a class may be defined only in a preinstantiated scope"))
         (const super class ((validate :inheritance) cxt env))
         (rwhen (not (& complete super))
           (throw-error -constant-error "cannot override a class before its definition has been compiled"))
         (rwhen (& final super)
           (throw-error -definition-error "can" :apostrophe "t override a " (:character-literal "final") " class"))
         (var a compound-attribute (to-compound-attribute attr))
         (rwhen (& prototype a)
           (throw-error -attribute-error "a class definition cannot have the " (:character-literal "prototype") " attribute"))
         (var final boolean)
         (case (& category a)
           (:select (tag none) (<- final false))
           (:select (tag static)
             (rwhen (not-in (nth env 0) class)
               (throw-error -attribute-error "non-class property definitions cannot have a " (:character-literal "static") " attribute"))
             (<- final false))
           (:select (tag final) (<- final true))
           (:select (tag virtual) (throw-error -attribute-error "a class definition cannot have the " (:character-literal "virtual") " attribute")))
         (const private-namespace namespace (new namespace "private"))
         (const dynamic boolean (or (& dynamic a) (and (& dynamic super) (/= super -object class))))
         (const c class (new class
                          (list-set-of local-binding) (list-set-of instance-property) super (&opt prototype super) false
                          (name :identifier) "object" private-namespace dynamic final null hint-number
                          (& has-property super) (& bracket-read super) (& bracket-write super) (& bracket-delete super)
                          (& read super) (& write super) (& delete super)
                          (& enumerate super)
                          ordinary-call ordinary-construct none ordinary-is ordinary-coerce))
         (action<- (class :class-definition 0) c)
         (const v variable (new variable -class c true none none :uninit))
         (exec (define-singleton-property env (name :identifier) (& namespaces a) (& override-mod a) (& explicit a) read-write v))
         (const inner-cxt context (new context (& strict cxt) (set+ (& open-namespaces cxt) (list-set private-namespace))))
         ((validate-using-frame :block) inner-cxt env (new jump-targets (list-set-of label) (list-set-of label)) preinst c)
         (when (in (& init c) (tag none))
           (&= init c (& init super)))
         (&= complete c true))
        
        ((setup)
         ((setup :block)))
        
        ((eval env d)
         (const c class (class :class-definition 0))
         (return ((eval-using-frame :block) env c d)))))
    
    
    (rule :inheritance ((validate (-> (context environment) class)))
      (production :inheritance () inheritance-none
        ((validate (cxt :unused) (env :unused)) (return -object)))
      (production :inheritance (extends (:type-expression allow-in)) inheritance-extends
        ((validate cxt env)
         ((validate :type-expression) cxt env)
         (return ((setup-and-eval :type-expression) env))))
      #|(production :inheritance (implements :type-expression-list) inheritance-implements
        ((validate (cxt :unused) (env :unused)) (return -object)))
      (production :inheritance (extends (:type-expression allow-in) implements :type-expression-list) inheritance-extends-implements
        ((validate (cxt :unused) (env :unused)) (return -object)))|#)
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
    (rule :namespace-definition ((validate (-> (context environment boolean attribute-opt-not-false) void)))
      (production :namespace-definition (namespace :identifier) namespace-definition-normal
        ((validate (cxt :unused) env preinst attr)
         (rwhen (not preinst)
           (throw-error -syntax-error "a namespace may be defined only in a preinstantiated scope"))
         (const a compound-attribute (to-compound-attribute attr))
         (rwhen (& dynamic a)
           (throw-error -attribute-error "a namespace definition cannot have the " (:character-literal "dynamic") " attribute"))
         (rwhen (& prototype a)
           (throw-error -attribute-error "a namespace definition cannot have the " (:character-literal "prototype") " attribute"))
         (case (& category a)
           (:select (tag none))
           (:select (tag static)
             (rwhen (not-in (nth env 0) class)
               (throw-error -attribute-error "non-class property definitions cannot have a " (:character-literal "static") " attribute")))
           (:select (tag virtual final)
             (throw-error -attribute-error
               "a namespace definition cannot have the " (:character-literal "virtual") " or " (:character-literal "final") " attribute")))
         (const name string (name :identifier))
         (const ns namespace (new namespace name))
         (const v variable (new variable -namespace ns true none none :uninit))
         (exec (define-singleton-property env name (& namespaces a) (& override-mod a) (& explicit a) read-write v)))))
    (%print-actions ("Validation" validate))
    
    
    (%heading 1 "Programs")
    (rule :program ((process object))
      (production :program (:directives) program-directives
        (process
         (begin
          (const cxt context (new context false (list-set public internal)))
          (const initial-environment environment (vector-of frame (create-global-object)))
          ((validate :directives) cxt initial-environment (new jump-targets (list-set-of label) (list-set-of label)) true none)
          ((setup :directives))
          (return ((eval :directives) initial-environment undefined)))))
      (production :program (:package-definition :program) program-package-and-program
        (process
         (begin
          (process :package-definition)
          (return (process :program))))))
    (%print-actions ("Processing" process))
    
    
    (%heading 2 "Package Definition")
    (rule :package-definition ((process void))
      (production :package-definition (package :package-name-opt :block) package-definition-name-and-block
        (process
         (begin
          (const name string (name :package-name-opt))
          (const cxt context (new context false (list-set public internal)))
          (const global-object package (create-global-object))
          (const pkg-internal namespace (new namespace "internal"))
          (const pkg package (new package
                               (list-set (std-explicit-const-binding (new qualified-name internal "internal") -namespace internal))
                               -object-prototype name busy true pkg-internal))
          (const initial-environment environment (vector-of frame pkg global-object))
          ((validate :block) cxt initial-environment (new jump-targets (list-set-of label) (list-set-of label)) true)
          ((setup :block))
          (function (eval-package) void
            (&= initialize pkg busy)
            (exec ((eval :block) initial-environment undefined))
            (&= initialize pkg none))
          (&= initialize pkg eval-package)
          (/* "Bind " (:local name) " to package " (:local pkg) " in the system" :apostrophe "s list of packages in an implementation-defined manner.")
          (<- package-database (set+ package-database (list-set pkg)))
          (*/)))))
    
    
    (rule :package-name-opt ((name string))
      (production :package-name-opt () package-name-opt-none
        (name (/*/ "" "an implementation-supplied name")))
      (production :package-name-opt (:package-name) package-name-opt-package-name
        (name (name :package-name))))
    
    (rule :package-name ((name string))
      (production :package-name ($string) package-name-string
        (name (/*/ (value $string) (:expr string (value $string)) " processed in an implementation-defined manner")))
      (production :package-name (:package-identifiers) package-name-package-identifiers
        (name (/*/ (nth (names :package-identifiers) 0) (:expr (vector string) (names :package-identifiers)) " processed in an implementation-defined manner"))))
    
    (rule :package-identifiers ((names (vector string)))
      (production :package-identifiers (:identifier) package-identifiers-one
        (names (vector (name :identifier))))
      (production :package-identifiers (:package-identifiers \. :identifier) package-identifiers-more
        (names (append (names :package-identifiers) (vector (name :identifier))))))
    (%print-actions ("Processing" process name names))
    
    (defvar package-database (list-set package) (list-set-of package))
    
    
    (%heading (1 :semantics) "Predefined Identifiers")
    (define (create-global-object) package
      (return
       (new package
         (%list-set
          (std-explicit-const-binding (new qualified-name internal "internal") -namespace internal)
          
          (std-const-binding (new qualified-name public "explicit") -attribute global_explicit)
          (std-const-binding (new qualified-name public "enumerable") -attribute global_enumerable)
          (std-const-binding (new qualified-name public "dynamic") -attribute global_dynamic)
          (std-const-binding (new qualified-name public "static") -attribute global_static)
          (std-const-binding (new qualified-name public "virtual") -attribute global_virtual)
          (std-const-binding (new qualified-name public "final") -attribute global_final)
          (std-const-binding (new qualified-name public "prototype") -attribute global_prototype)
          (std-const-binding (new qualified-name public "unused") -attribute global_unused)
          (std-function (new qualified-name public "override") global_override 1)
          
          (std-const-binding (new qualified-name public "NaN") -number nan64)
          (std-const-binding (new qualified-name public "Infinity") -number +infinity64)
          (std-const-binding (new qualified-name public "fNaN") float nan32)
          (std-const-binding (new qualified-name public "fInfinity") float +infinity32)
          (std-const-binding (new qualified-name public "undefined") -void undefined)
          
          (std-function (new qualified-name public "eval") global_eval 1)
          (std-function (new qualified-name public "parseInt") global_parseInt 2)
          (std-function (new qualified-name public "parseLong") global_parseLong 2)
          (std-function (new qualified-name public "parseFloat") global_parseFloat 1)
          (std-function (new qualified-name public "isNaN") global_isNaN 1)
          (std-function (new qualified-name public "isFinite") global_isFinite 1)
          (std-function (new qualified-name public "decodeURI") global_decodeURI 1)
          (std-function (new qualified-name public "decodeURIComponent") global_decodeURIComponent 1)
          (std-function (new qualified-name public "encodeURI") global_encodeURI 1)
          (std-function (new qualified-name public "encodeURIComponent") global_encodeURIComponent 1)
          
          (std-const-binding (new qualified-name public "Object") -class -object)
          (std-const-binding (new qualified-name public "Never") -class -never)
          (std-const-binding (new qualified-name public "Void") -class -void)
          (std-const-binding (new qualified-name public "Null") -class -null)
          (std-const-binding (new qualified-name public "Boolean") -class -boolean)
          (std-const-binding (new qualified-name public "GeneralNumber") -class -general-number)
          (std-const-binding (new qualified-name public "long") -class \#long)
          (std-const-binding (new qualified-name public "ulong") -class ulong)
          (std-const-binding (new qualified-name public "float") -class float)
          (std-const-binding (new qualified-name public "Number") -class -number)
          (std-const-binding (new qualified-name public "sbyte") -class sbyte)
          (std-const-binding (new qualified-name public "byte") -class byte)
          (std-const-binding (new qualified-name public "short") -class short)
          (std-const-binding (new qualified-name public "ushort") -class ushort)
          (std-const-binding (new qualified-name public "int") -class int)
          (std-const-binding (new qualified-name public "uint") -class uint)
          (std-const-binding (new qualified-name public "char") -class char)
          (std-const-binding (new qualified-name public "String") -class -string)
          (std-const-binding (new qualified-name public "Array") -class -array)
          (std-const-binding (new qualified-name public "Namespace") -class -namespace)
          (std-const-binding (new qualified-name public "Attribute") -class -attribute)
          (std-const-binding (new qualified-name public "Date") -class -date)
          (std-const-binding (new qualified-name public "RegExp") -class -reg-exp)
          (std-const-binding (new qualified-name public "Class") -class -class)
          (std-const-binding (new qualified-name public "Function") -class -function)
          (std-const-binding (new qualified-name public "PrototypeFunction") -class -prototype-function)
          (std-const-binding (new qualified-name public "Package") -class -package)
          (std-const-binding (new qualified-name public "Error") -class -error)
          (std-const-binding (new qualified-name public "ArgumentError") -class -argument-error)
          (std-const-binding (new qualified-name public "AttributeError") -class -attribute-error)
          (std-const-binding (new qualified-name public "ConstantError") -class -constant-error)
          (std-const-binding (new qualified-name public "DefinitionError") -class -definition-error)
          (std-const-binding (new qualified-name public "EvalError") -class -eval-error)
          (std-const-binding (new qualified-name public "RangeError") -class -range-error)
          (std-const-binding (new qualified-name public "ReferenceError") -class -reference-error)
          (std-const-binding (new qualified-name public "SyntaxError") -class -syntax-error)
          (std-const-binding (new qualified-name public "TypeError") -class -type-error)
          (std-const-binding (new qualified-name public "UninitializedError") -class -uninitialized-error)
          (std-const-binding (new qualified-name public "URIError") -class -u-r-i-error)
          )
         -object-prototype "" none false internal)))
    
    
    (%heading (2 :semantics) "Built-in Namespaces")
    (define public namespace (new namespace "public"))
    (define internal namespace (new namespace "internal"))
    
    (%heading (2 :semantics) "Built-in Attributes")
    (define global_explicit compound-attribute (new compound-attribute (list-set-of namespace) true false false none none false false))
    (define global_enumerable compound-attribute (new compound-attribute (list-set-of namespace) false true false none none false false))
    (define global_dynamic compound-attribute (new compound-attribute (list-set-of namespace) false false true none none false false))
    (define global_static compound-attribute (new compound-attribute (list-set-of namespace) false false false static none false false))
    (define global_virtual compound-attribute (new compound-attribute (list-set-of namespace) false false false virtual none false false))
    (define global_final compound-attribute (new compound-attribute (list-set-of namespace) false false false final none false false))
    (define global_prototype compound-attribute (new compound-attribute (list-set-of namespace) false false false none none true false))
    (define global_unused compound-attribute (new compound-attribute (list-set-of namespace) false false false none none false true))
    (define (global_override (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase :unused)) object
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (var override-mod override-modifier)
      (cond
       ((empty args) (<- override-mod true))
       ((= (length args) 1)
        (const arg object (nth args 0))
        (rwhen (not-in arg (tag true false undefined) :narrow-false)
          (throw-error -type-error))
        (<- override-mod arg))
       (nil (throw-error -argument-error "too many arguments supplied")))
      (return (new compound-attribute (list-set-of namespace) false false false none override-mod false false)))
    
    
    (%heading (2 :semantics) "Built-in Functions")
    
    (define (global_eval (this object :unused) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) object
      (todo))
    
    
    (define (global_parseInt (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase)) float64
      (note "This function can be used in a constant expression if the arguments can be converted to primitives in constant expressions.")
      (rwhen (set-not-in (length args) (range-set-of integer 1 2))
        (throw-error -argument-error "at least one and at most two arguments must be supplied"))
      (const s string (object-to-string (nth args 0) phase))
      (var radix integer (object-to-integer (default-arg args 1 +zero64) phase))
      (var i (union (exclude-zero integer) (tag +zero -zero nan)) (string-prefix-to-integer s radix))
      (return (extended-rational-to-float64 i)))
    
    (define (string-prefix-to-integer (s string) (radix integer))
            (union (exclude-zero integer) (tag +zero -zero nan))
      (var r integer radix)
      (rwhen (set-not-in r (range-set-of-ranges integer 0 nil 2 36))
        (throw-error -range-error "radix out of range"))
      (var i integer 0)
      (while (and (< i (length s))
                  (lisp-call white-space-or-line-terminator-char? ((nth s i))
                             boolean
                    "the nonterminal " (:grammar-symbol :white-space-or-line-terminator-char nil "lexer-semantics.html") " can expand into " (:expr string (vector (nth s i)))))
        (<- i (+ i 1)))
      (var sign (integer-list -1 1) 1)
      (when (< i (length s))
        (cond
         ((= (nth s i) #\+ char16)
          (<- i (+ i 1)))
         ((= (nth s i) #\- char16)
          (<- sign -1)
          (<- i (+ i 1)))))
      (when (and (set-in r (list-set 0 16))
                 (<= (+ i 2) (length s))
                 (set-in (subseq s i (+ i 1)) (list-set "0x" "0X")))
        (<- r 16)
        (<- i (+ i 2)))
      (when (= r 0)
        (<- r 10))
      (var n integer 0)
      (const start integer i)
      (var digit integer-opt 0)
      (while (and (< i (length s)) (not-in digit (tag none)))
        (const ch char16 (nth s i))
        (cond
         ((set-in ch (range-set-of-ranges char16 #\0 #\9))
          (<- digit (- (char16-to-integer ch) (char16-to-integer #\0))))
         ((set-in ch (range-set-of-ranges char16 #\A #\Z))
          (<- digit (+ (- (char16-to-integer ch) (char16-to-integer #\A)) 10)))
         ((set-in ch (range-set-of-ranges char16 #\a #\z))
          (<- digit (+ (- (char16-to-integer ch) (char16-to-integer #\a)) 10)))
         (nil (<- digit none)))
        (when (and (not-in digit (tag none) :narrow-true) (>= digit r))
          (<- digit none :end-narrow))
        (when (not-in digit (tag none) :narrow-true)
          (<- n (+ (* n r) digit))
          (<- i (+ i 1))))
      (rwhen (= i start)
        (return nan))
      (cond
       ((/= n 0) (return (* n sign)))
       ((> sign 0) (return +zero))
       (nil (return -zero))))
      
    (define (global_parseLong (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase)) general-number
      (note "This function can be used in a constant expression if the arguments can be converted to primitives in constant expressions.")
      (rwhen (set-not-in (length args) (range-set-of integer 1 2))
        (throw-error -argument-error "at least one and at most two arguments must be supplied"))
      (const s string (object-to-string (nth args 0) phase))
      (var radix integer (object-to-integer (default-arg args 1 +zero64) phase))
      (var i (union (exclude-zero integer) (tag +zero -zero nan)) (string-prefix-to-integer s radix))
      (case i
        (:select (tag +zero -zero) (return (new long 0)))
        (:narrow integer (return (integer-to-long i)))
        (:select (tag nan) (return nan64))))
    
    
    (define (global_parseFloat (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase)) float64
      (note "This function can be used in a constant expression if its argument can be converted to a primitive in a constant expression.")
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const s string (object-to-string (nth args 0) phase))
      (/* "Apply the lexer grammar with the start symbol " (:grammar-symbol :string-decimal-literal nil "lexer-semantics.html") " to the string " (:local s)
          ". If the grammar can interpret neither " (:local s) " nor any prefix of " (:local s) " as an expansion of "
          (:grammar-symbol :string-decimal-literal nil "lexer-semantics.html") ", then " (:keyword return) :nbsp (:tag nan64)
          ". Otherwise, let " (:local p) " be the longest prefix of " (:local s) " (possibly " (:local s) " itself) such that " (:local p)
          " is an expansion of " (:grammar-symbol :string-decimal-literal nil "lexer-semantics.html") ".")
      (const q (union extended-rational (tag syntax-error))
        (lisp-call string-prefix-to-float (s)
                   (union extended-rational (tag syntax-error))
          "the result of lexing the longest possible prefix of " (:operand 0) " using " (:grammar-symbol :string-decimal-literal nil "lexer-semantics.html")
          " as the start symbol"))
      (rwhen (in q (tag syntax-error) :narrow-false)
        (return nan64))
      (*/)
      (// (:local q) ":" :nbsp (:type extended-rational) " " :assign-10 " the value of the action " (:action lex) " applied to "
          (:local p) :apostrophe "s expansion of the nonterminal " (:grammar-symbol :string-decimal-literal nil "lexer-semantics.html") ";")
      (return (extended-rational-to-float64 q)))
    
    
    (define (global_isNaN (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase)) \#boolean
      (note "This function can be used in a constant expression if its argument can be converted to a primitive in a constant expression.")
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const x general-number (object-to-general-number (nth args 0) phase))
      (return (in x (tag nan32 nan64))))
    
    
    (define (global_isFinite (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase)) \#boolean
      (note "This function can be used in a constant expression if its argument can be converted to a primitive in a constant expression.")
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const x general-number (object-to-general-number (nth args 0) phase))
      (return (not-in x (tag nan32 nan64 +infinity32 +infinity64 -infinity32 -infinity64))))
    
    
    (define (global_decodeURI (this object :unused) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) object
      (todo))
    
    
    (define (global_decodeURIComponent (this object :unused) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) object
      (todo))
    
    
    (define (global_encodeURI (this object :unused) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) object
      (todo))
    
    
    (define (global_encodeURIComponent (this object :unused) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) object
      (todo))
    
    
    
    (%heading (1 :semantics) "Built-in Classes")
    (define (dummy-call (this object :unused) (c class :unused) (args (vector object) :unused) (phase phase :unused)) object
      (todo))
    (define (dummy-construct (c class :unused) (args (vector object) :unused) (phase phase :unused)) object
      (todo))
    
    
    (define prototypes-sealed boolean false)
    
    
    (%heading (2 :semantics) "Object")
    (define -object class
      (new class
        (list-set-of local-binding)
        (list-set-of instance-property)
        none (delay -object-prototype) true
        "Object" "object" :uninit true false undefined hint-number
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        call-object construct-object none ordinary-is coerce-object))
    
    (define (call-object (this object :unused) (c class :unused) (args (vector object)) (phase phase :unused)) object
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (cond
       ((= (length args) 0) (return undefined))
       ((= (length args) 1) (return (nth args 0)))
       (nil (throw-error -argument-error "at most one argument can be supplied"))))
    
    (define (construct-object (c class :unused) (args (vector object)) (phase phase :unused)) object
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const o object (default-arg args 0 undefined))
      (if (in o (tag null undefined))
        (return (create-simple-instance -object -object-prototype none none none))
        (return o)))
    
    (define (coerce-object (o object) (c class :unused)) object-opt
      (return o))
    
    (define -object-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -object)
         (std-function (new qualified-name public "toString") -object_to-string 0)
         (std-function (new qualified-name public "toLocaleString") -object_to-locale-string 0)
         (std-function (new qualified-name public "valueOf") -object_value-of 0)
         (std-function (new qualified-name public "hasOwnProperty") -object_has-own-property 1)
         (std-function (new qualified-name public "isPrototypeOf") -object_is-prototype-of 1)
         (std-function (new qualified-name public "propertyIsEnumerable") -object_property-is-enumerable 1)
         (std-function (new qualified-name public "sealProperty") -object_seal-property 1))
        none prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (define (-object_to-string (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) string
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (const c class (object-type this))
      (return (append "[object " (& name c) "]")))
    
    
    (define (-object_to-locale-string (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "toLocaleString") " cannot be called from a constant expression"))
      (const to-string-method object (dot-read this (list-set (new qualified-name public "toString")) phase))
      (return (call this to-string-method args phase)))
    
    
    (define (-object_value-of (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) object
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (return this))
    
    
    (define (-object_has-own-property (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) \#boolean
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "hasOwnProperty") " cannot be called from a constant expression"))
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (return (has-property this (nth args 0) true phase)))
    
    
    (define (-object_is-prototype-of (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) \#boolean
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "isPrototypeOf") " cannot be called from a constant expression"))
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const o object (nth args 0))
      (return (set-in this (archetypes o))))
    
    
    (define (-object_property-is-enumerable (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) \#boolean
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "propertyIsEnumerable") " cannot be called from a constant expression"))
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const qname qualified-name (object-to-qualified-name (nth args 0) phase))
      (const c class (object-type this))
      (var m-base instance-property-opt (find-base-instance-property c (list-set qname) read))
      (when (not-in m-base (tag none) :narrow-true)
        (const m instance-property (get-derived-instance-property c m-base read))
        (rwhen (&opt enumerable m)
          (return true)))
      (<- m-base (find-base-instance-property c (list-set qname) write))
      (when (not-in m-base (tag none) :narrow-true)
        (const m instance-property (get-derived-instance-property c m-base write))
        (rwhen (&opt enumerable m)
          (return true)))
      (rwhen (not-in this binding-object :narrow-false)
        (return false))
      (return (some (& local-bindings this) b (and (= (& qname b) qname qualified-name) (& enumerable b)))))
    
    
    (define (-object_seal-property (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) undefined
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "sealProperty") " cannot be called from a constant expression"))
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const arg object (default-arg args 0 true))
      (cond
       ((in arg (tag false) :narrow-false)
        (seal-object this))
       ((in arg (tag true) :narrow-false)
        (seal-object this)
        (seal-all-local-properties this))
       ((in arg (union char16 string))
        (rwhen (not (has-property this arg true phase))
          (throw-error -reference-error "property not found"))
        (const qname qualified-name (object-to-qualified-name arg phase))
        (seal-local-property this qname)))
      (return undefined))

    
    
    (%heading (2 :semantics) "Never")
    (define -never class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object none true
        "Never" "" :uninit false true none :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-never none ordinary-is coerce-never))
    
    (define (construct-never (c class :unused) (args (vector object)) (phase phase :unused)) object
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (throw-error -type-error "no coercions to " (:character-literal "Never") " are possible"))
    
    (define (coerce-never (o object :unused) (c class :unused)) (tag none)
      (return none))
    
    
    (%heading (2 :semantics) "Void")
    (define -void class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object none true
        "Void" "undefined" :uninit false true undefined :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        call-void construct-void none ordinary-is coerce-void))
    
    (define (call-void (this object :unused) (c class :unused) (args (vector object)) (phase phase :unused)) undefined
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (return undefined))
    
    (define (construct-void (c class :unused) (args (vector object)) (phase phase :unused)) undefined
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (rwhen (/= (length args) 0)
        (throw-error -argument-error "no arguments can be supplied"))
      (return undefined))
    
    (define (coerce-void (o object) (c class :unused)) (tag undefined none)
      (if (in o (union null undefined))
        (return undefined)
        (return none)))
    
    
    (%heading (2 :semantics) "Null")
    (define -null class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object none true
        "Null" "object" :uninit false true null :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        call-null construct-null none ordinary-is coerce-null))
    
    (define (call-null (this object :unused) (c class :unused) (args (vector object)) (phase phase :unused)) null
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (return null))
    
    (define (construct-null (c class :unused) (args (vector object)) (phase phase :unused)) null
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (rwhen (/= (length args) 0)
        (throw-error -argument-error "no arguments can be supplied"))
      (return null))
    
    (define (coerce-null (o object) (c class :unused)) (tag null none)
      (if (in o (tag null) :narrow-true)
        (return o)
        (return none)))
    
    
    (%heading (2 :semantics) "Boolean")
    (define -boolean class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object (delay -boolean-prototype) true
        "Boolean" "boolean" :uninit false true false :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-boolean none ordinary-is coerce-boolean))
    
    (define (construct-boolean (c class :unused) (args (vector object)) (phase phase :unused)) \#boolean
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (return (object-to-boolean (default-arg args 0 false))))
    
    (define (coerce-boolean (o object) (c class :unused)) boolean-opt
      (if (in o boolean :narrow-true)
        (return o)
        (return none)))
    
    (define -boolean-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -boolean)
         (std-function (new qualified-name public "toString") -boolean_to-string 0)
         (std-reserve (new qualified-name public "valueOf") -object-prototype))
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (define (-boolean_to-string (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (note "This function can be used in a constant expression.")
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (const a boolean (object-to-boolean this))
      (return (object-to-string a phase)))
    
    
    (%heading (2 :semantics) "GeneralNumber")
    (define -general-number class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object (delay -general-number-prototype) true
        "GeneralNumber" "object" :uninit false true nan64 hint-number
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-general-number none ordinary-is coerce-general-number))
    
    (define (construct-general-number (c class :unused) (args (vector object)) (phase phase)) general-number
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (cond
       ((= (length args) 0) (return +zero64))
       ((= (length args) 1) (return (object-to-general-number (nth args 0) phase)))
       (nil (throw-error -argument-error "at most one argument can be supplied"))))
    
    (define (coerce-general-number (o object) (c class :unused)) (union general-number (tag none))
      (if (in o general-number :narrow-true)
        (return o)
        (return none)))
    
    (define -general-number-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -general-number)
         (std-function (new qualified-name public "toString") -general-number_to-string 1)
         (std-reserve (new qualified-name public "valueOf") -object-prototype)
         (std-function (new qualified-name public "toFixed") -general-number_to-fixed 1)
         (std-function (new qualified-name public "toExponential") -general-number_to-exponential 1)
         (std-function (new qualified-name public "toPrecision") -general-number_to-precision 1))
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (define (-general-number_to-string (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the argument can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a general number.")
      (const x general-number (object-to-general-number this phase))
      (var radix integer (object-to-integer (default-arg args 0 10.0) phase))
      (rwhen (or (< radix 2) (> radix 36))
        (throw-error -range-error "bad radix"))
      (if (= radix 10)
        (return (general-number-to-string x))
        (return (/*/ "*****Implementation-defined" (:local x) " converted to a string containing a base-" (:local radix) " number in an implementation-defined manner"))))
    
    
    (define precision-limit integer (/*/ 100 "an implementation-defined integer not less than 20"))
    
    (define (-general-number_to-fixed (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the argument can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a general number.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const x general-number (object-to-general-number this phase))
      (var fraction-digits integer (object-to-integer (default-arg args 0 +zero64) phase))
      (rwhen (or (< fraction-digits 0) (> fraction-digits precision-limit))
        (throw-error -range-error))
      (rwhen (not-in x finite-general-number :narrow-false)
        (return (general-number-to-string x)))
      (var r rational (to-rational x))
      (when (>= (rat-abs r) (expt 10 21) rational)
        (return (general-number-to-string x)))
      (var sign string "")
      (when (< r 0 rational)
        (<- sign "-")
        (<- r (rat-neg r)))
      (const n integer (floor (rat+ (rat* r (expt 10 fraction-digits)) (rat/ 1 2))))
      (var digits string (integer-to-string n))
      (cond
       ((= fraction-digits 0)
        (return (append sign digits)))
       (nil
        (when (<= (length digits) fraction-digits)
          (<- digits (append (repeat char16 #\0 (- (+ fraction-digits 1) (length digits))) digits)))
        (const k integer (- (length digits) fraction-digits))
        (return (append sign (subseq digits 0 (- k 1)) "." (subseq digits k))))))
    
    
    (define (-general-number_to-exponential (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the argument can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a general number.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const x general-number (object-to-general-number this phase))
      (var fraction-digits extended-integer (object-to-extended-integer (default-arg args 0 nan64) phase))
      (rwhen (or (in fraction-digits (tag +infinity -infinity) :narrow-false) (and (not-in fraction-digits (tag nan) :narrow-true)
                                                                                   (or (< fraction-digits 0) (> fraction-digits precision-limit))))
        (throw-error -range-error))
      (rwhen (not-in x finite-general-number :narrow-false)
        (return (general-number-to-string x)))
      (var r rational (to-rational x))
      (var sign string "")
      (when (< r 0 rational)
        (<- sign "-")
        (<- r (rat-neg r)))
      (var digits string)
      (var e integer)
      (cond
       ((not-in fraction-digits (tag nan) :narrow-true)
        (cond
         ((= r 0 rational)
          (<- digits (repeat char16 #\0 (+ fraction-digits 1)))
          (<- e 0))
         (nil
          (<- e (floor-log10 r))
          (var n integer (floor (rat+ (rat* r (expt 10 (- fraction-digits e))) (rat/ 1 2))))
          (assert (cascade rational (expt 10 fraction-digits) <= n <= (expt 10 (+ fraction-digits 1))) "At this point " (:assertion))
          (when (= n (expt 10 (+ fraction-digits 1)) rational)
            (<- n (int/ n 10))
            (<- e (+ e 1)))
          (<- digits (integer-to-string n))))
        (assert (= (length digits) (+ fraction-digits 1)) "At this point the string " (:local digits) " has exactly " (:expr integer (+ fraction-digits 1)) " digits"))
       ((= r 0 rational)
        (<- digits "0")
        (<- e 0))
       ((in x (union long u-long) :narrow-false)
        (<- digits (integer-to-string (assert-in r integer)))
        (<- e (- (length digits) 1))
        (while (= (nth digits (- (length digits) 1)) #\0 char16)
          (<- digits (subseq digits 0 (- (length digits) 2)))))
       (nil
        (quiet-assert (not-in x (tag +zero32 -zero32 +zero64 -zero64) :narrow-true))
        (var k integer)
        (var s integer)
        (case x
          (:narrow nonzero-finite-float32
            (/* (:initialize e) (:initialize k) (:initialize s)
                "Let " (:local e) ", " (:local k) ", and " (:local s) " be integers such that "
                (:expr boolean (>= k 1)) ", " (:expr boolean (cascade rational (expt 10 (- k 1)) <= s <= (expt 10 k))) ", "
                (:expr boolean (= (real-to-float32 (rat* s (expt 10 (- (+ e 1) k)))) x float32)) ", and " (:local k) " is as small as possible.")
            (multiple-value-bind ((digits2 string) (e2 integer)) decompose-positive-float32 ((if (< r 0 rational) (float32-negate x) x)))
            (<- digits digits2)
            (<- e e2)
            (<- k (length digits))
            (<- s k) ;Shut up unreference variable warnings
            (<- k s)
            (*/))
          (:narrow nonzero-finite-float64
            (/* (:initialize e) (:initialize k) (:initialize s)
                "Let " (:local e) ", " (:local k) ", and " (:local s) " be integers such that "
                (:expr boolean (>= k 1)) ", " (:expr boolean (cascade rational (expt 10 (- k 1)) <= s <= (expt 10 k))) ", "
                (:expr boolean (= (real-to-float64 (rat* s (expt 10 (- (+ e 1) k)))) x float64)) ", and " (:local k) " is as small as possible.")
            (multiple-value-bind ((digits2 string) (e2 integer)) decompose-positive-float64 ((if (< r 0 rational) (float64-negate x) x)))
            (<- digits digits2)
            (<- e e2)
            (<- k (length digits))
            (<- s k) ;Shut up unreference variable warnings
            (<- k s)
            (*/)))
        (note (:local k) " is the number of digits in the decimal representation of " (:local s) ", " (:local s)
              " is not divisible by 10, and the least significant digit of " (:local s)
              " is not necessarily uniquely determined by the above criteria.")
        (// "When there are multiple possibilities for " (:local s) " according to the rules above, "
            "implementations are encouraged but not required to select the one according to the following rules: "
            "Select the value of " (:local s) " for which " (:expr rational (rat* s (expt 10 (- (+ e 1) k)))) " is closest in value to " (:local r)
            "; if there are two such possible values of " (:local s) ", choose the one that is even.")
        (// (:local digits) :nbsp :assign-10 :nbsp (:expr string (integer-to-string s)))))
      (return (append sign (exponential-notation-string digits e))))
    
    
    (define (-general-number_to-precision (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the argument can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a general number.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const x general-number (object-to-general-number this phase))
      (var precision extended-integer (object-to-extended-integer (default-arg args 0 nan64) phase))
      (rwhen (in precision (tag nan) :narrow-false)
        (return (general-number-to-string x)))
      (rwhen (or (in precision (tag +infinity -infinity) :narrow-false) (< precision 1) (> precision (+ precision-limit 1)))
        (throw-error -range-error))
      (rwhen (not-in x finite-general-number :narrow-false)
        (return (general-number-to-string x)))
      (var r rational (to-rational x))
      (var sign string "")
      (when (< r 0 rational)
        (<- sign "-")
        (<- r (rat-neg r)))
      (var digits string)
      (var e integer)
      (cond
       ((= r 0 rational)
        (<- digits (repeat char16 #\0 precision))
        (<- e 0))
       (nil
        (<- e (floor-log10 r))
        (var n integer (floor (rat+ (rat* r (expt 10 (- (- precision 1) e))) (rat/ 1 2))))
        (assert (cascade rational (expt 10 (- precision 1)) <= n <= (expt 10 precision)) "At this point " (:assertion))
        (when (= n (expt 10 precision) rational)
          (<- n (int/ n 10))
          (<- e (+ e 1)))
        (<- digits (integer-to-string n))))
      (assert (= (length digits) precision) "At this point the string " (:local digits) " has exactly " (:local precision) " digits")
      (cond
       ((or (< e -6) (>= e precision))
        (return (append sign (exponential-notation-string digits e))))
       ((= e (- precision 1))
        (return (append sign digits)))
       ((>= e 0)
        (return (append sign (subseq digits 0 e) "." (subseq digits (+ e 1)))))
       (nil
        (return (append sign "0." (repeat char16 #\0 (neg (+ e 1))) digits)))))
    
    
    (%heading (2 :semantics) "long")
    (define \#long class
      (new class
        (%list-set
         (std-const-binding (new qualified-name public "MAX_VALUE") (delay ulong) (new long (- (expt 2 63) 1)))
         (std-const-binding (new qualified-name public "MIN_VALUE") (delay ulong) (new long (neg (expt 2 63)))))
        (list-set-of instance-property) -general-number (delay long-prototype) true
        "long" "long" :uninit false true (new long 0) :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-long none ordinary-is coerce-long))
    
    (define (construct-long (c class :unused) (args (vector object)) (phase phase)) long
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const arg object (default-arg args 0 +zero64))
      (const i integer (object-to-integer arg phase))
      (if (cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 63) 1))
        (return (new long i))
        (throw-error -range-error (:local i) " is out of the " (:type long) " range")))
    
    (define (coerce-long (o object) (c class :unused)) (union long (tag none))
      (rwhen (not-in o general-number :narrow-false)
        (return none))
      (const i integer-opt (check-integer o))
      (if (and (not-in i (tag none) :narrow-true) (cascade integer (neg (expt 2 63)) <= i <= (- (expt 2 63) 1)))
        (return (new long i))
        (throw-error -range-error (:local i) " is out of the " (:type long) " range")))
    
    (define long-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class \#long)
         (std-reserve (new qualified-name public "toString") -general-number-prototype)
         (std-reserve (new qualified-name public "valueOf") -general-number-prototype))
        -general-number-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (%heading (2 :semantics) "ulong")
    (define ulong class
      (new class
        (%list-set
         (std-const-binding (new qualified-name public "MAX_VALUE") (delay ulong) (new u-long (- (expt 2 64) 1)))
         (std-const-binding (new qualified-name public "MIN_VALUE") (delay ulong) (new u-long 0)))
        (list-set-of instance-property) -general-number (delay ulong-prototype) true
        "ulong" "ulong" :uninit false true (new u-long 0) :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-u-long none ordinary-is coerce-u-long))
    
    (define (construct-u-long (c class :unused) (args (vector object)) (phase phase)) u-long
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const arg object (default-arg args 0 +zero64))
      (const i integer (object-to-integer arg phase))
      (if (cascade integer 0 <= i <= (- (expt 2 64) 1))
        (return (new u-long i))
        (throw-error -range-error (:local i) " is out of the " (:type u-long) " range")))
    
    (define (coerce-u-long (o object) (c class :unused)) (union u-long (tag none))
      (rwhen (not-in o general-number :narrow-false)
        (return none))
      (const i integer-opt (check-integer o))
      (if (and (not-in i (tag none) :narrow-true) (cascade integer 0 <= i <= (- (expt 2 64) 1)))
        (return (new u-long i))
        (throw-error -range-error (:local i) " is out of the " (:type u-long) " range")))
    
    (define ulong-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class ulong)
         (std-reserve (new qualified-name public "toString") -general-number-prototype)
         (std-reserve (new qualified-name public "valueOf") -general-number-prototype))
        -general-number-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (%heading (2 :semantics) "float")
    (define float class
      (new class
        (%list-set
         (std-const-binding (new qualified-name public "MAX_VALUE") (delay float) (float32 3.4028235e+38))
         (std-const-binding (new qualified-name public "MIN_VALUE") (delay float) (float32 1e-45))
         (std-const-binding (new qualified-name public "NaN") (delay float) nan32)
         (std-const-binding (new qualified-name public "NEGATIVE_INFINITY") (delay float) -infinity32)
         (std-const-binding (new qualified-name public "POSITIVE_INFINITY") (delay float) +infinity32))
        (list-set-of instance-property) -general-number (delay float-prototype) true
        "float" "float" :uninit false true nan32 :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-float none ordinary-is coerce-float))
    
    (define (construct-float (c class :unused) (args (vector object)) (phase phase)) float32
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (cond
       ((= (length args) 0) (return +zero32))
       ((= (length args) 1) (return (object-to-float32 (nth args 0) phase)))
       (nil (throw-error -argument-error "at most one argument can be supplied"))))
    
    (define (coerce-float (o object) (c class :unused)) (union float32 (tag none))
      (if (in o general-number :narrow-true)
        (return (to-float32 o))
        (return none)))
    
    (define float-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class float)
         (std-reserve (new qualified-name public "toString") -general-number-prototype)
         (std-reserve (new qualified-name public "valueOf") -general-number-prototype))
        -general-number-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (%heading (2 :semantics) "Number")
    (define -number class
      (new class
        (%list-set
         (std-const-binding (new qualified-name public "MAX_VALUE") (delay -number) 1.7976931348623157e+308)
         (std-const-binding (new qualified-name public "MIN_VALUE") (delay -number) 5e-324)
         (std-const-binding (new qualified-name public "NaN") (delay -number) nan64)
         (std-const-binding (new qualified-name public "NEGATIVE_INFINITY") (delay -number) -infinity64)
         (std-const-binding (new qualified-name public "POSITIVE_INFINITY") (delay -number) +infinity64))
        (list-set-of instance-property) -general-number (delay -number-prototype) true
        "Number" "number" :uninit false true nan64 :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-number none ordinary-is coerce-number))
    
    (define (construct-number (c class :unused) (args (vector object)) (phase phase)) float64
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (cond
       ((= (length args) 0) (return +zero64))
       ((= (length args) 1) (return (object-to-float64 (nth args 0) phase)))
       (nil (throw-error -argument-error "at most one argument can be supplied"))))
    
    (define (coerce-number (o object) (c class :unused)) (union float64 (tag none))
      (if (in o general-number :narrow-true)
        (return (to-float64 o))
        (return none)))
    
    (define -number-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -number)
         (std-reserve (new qualified-name public "toString") -general-number-prototype)
         (std-reserve (new qualified-name public "valueOf") -general-number-prototype))
        -general-number-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    
    (define (make-built-in-integer-class (name string) (low integer) (high integer)) class
      (function (construct (c class :unused) (args (vector object)) (phase phase)) float64
        (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
        (rwhen (> (length args) 1)
          (throw-error -argument-error "at most one argument can be supplied"))
        (const arg object (default-arg args 0 +zero64))
        (const x float64 (object-to-float64 arg phase))
        (const i integer-opt (check-integer x))
        (rwhen (and (not-in i (tag none) :narrow-true) (cascade integer low <= i <= high))
          (note (:tag -zero64) " is coerced to " (:tag +zero64) ".")
          (return (real-to-float64 i)))
        (throw-error -range-error))
      (function (is (o object) (c class :unused)) boolean
        (rwhen (not-in o float64 :narrow-false)
          (return false))
        (const i integer-opt (check-integer o))
        (return (and (not-in i (tag none) :narrow-true) (cascade integer low <= i <= high))))
      (function (coerce (o object) (c class :unused)) (union float64 (tag none))
        (rwhen (not-in o general-number :narrow-false)
          (return none))
        (const i integer-opt (check-integer o))
        (rwhen (and (not-in i (tag none) :narrow-true) (cascade integer low <= i <= high))
          (note (:tag -zero32) ", " (:tag +zero32) ", and " (:tag -zero64) " are all coerced to " (:tag +zero64) ".")
          (return (real-to-float64 i)))
        (throw-error -range-error))
      (return (new class
                (%list-set
                 (std-const-binding (new qualified-name public "MAX_VALUE") -number (real-to-float64 high))
                 (std-const-binding (new qualified-name public "MIN_VALUE") -number (real-to-float64 low)))
                (list-set-of instance-property) -number (&opt prototype -number) true
                name "number" :uninit false true +zero64 :uninit
                (& has-property -number) (& bracket-read -number) (& bracket-write -number) (& bracket-delete -number)
                (& read -number) (& write -number) (& delete -number)
                (& enumerate -number)
                same-as-construct construct none is coerce)))
    
    (define sbyte class (make-built-in-integer-class "sbyte" -128 127))
    (define byte class (make-built-in-integer-class "byte" 0 255))
    (define short class (make-built-in-integer-class "short" -32768 32767))
    (define ushort class (make-built-in-integer-class "ushort" 0 65535))
    (define int class (make-built-in-integer-class "int" -2147483648 2147483647))
    (define uint class (make-built-in-integer-class "uint" 0 4294967295))
    
    
    
    (%heading (2 :semantics) "char")
    (define char class
      (new class
        (list-set (std-function (new qualified-name public "fromCharCode") char_from-char-code 1))
        (list-set-of instance-property) -object (delay char-prototype) true
        "char" "char" :uninit false true #?0000 :uninit
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-char none ordinary-is coerce-char))
    
    (define (call-char (this object :unused) (c class :unused) (args (vector object)) (phase phase)) char16
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const s string (object-to-string (nth args 0) phase))
      (rwhen (/= (length s) 1)
        (throw-error -range-error "only one character may be given"))
      (return (nth s 0)))
    
    (define (construct-char (c class :unused) (args (vector object)) (phase phase)) char16
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const arg object (default-arg args 0 undefined))
      (cond
       ((in arg (tag undefined)) (return #?0000))
       ((in arg char16 :narrow-true) (return arg))
       (nil
        (const s string (object-to-string (nth args 0) phase))
        (rwhen (/= (length s) 1)
          (throw-error -range-error "only one character may be given"))
        (return (nth s 0)))))
    
    (define (coerce-char (o object) (c class :unused)) (union char16 (tag none))
      (if (in o char16 :narrow-true)
        (return o)
        (return none)))
    
    
    (define (char_from-char-code (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const i integer (object-to-integer (nth args 0) phase))
      (if (cascade integer 0 <= i <= (hex #xFFFF))
        (return (integer-to-char16 i))
        (throw-error -range-error "character code out of range")))
    
    
    (define char-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class char)
         (std-reserve (new qualified-name public "toString") -string-prototype)
         (std-reserve (new qualified-name public "valueOf") -string-prototype))
        -string-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    
    (%heading (2 :semantics) "String")
    (define -string class
      (new class
        (list-set (std-function (new qualified-name public "fromCharCode") -string_from-char-code 1))
        (%list-set-of instance-property
          (new instance-getter (list-set (new qualified-name public "length")) true false :uninit -string_length))
        -object (delay -string-prototype) true
        "String" "string" :uninit false true null :uninit
        string-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete read-string ordinary-write ordinary-delete ordinary-enumerate
        same-as-construct construct-string none ordinary-is coerce-string))
    
    (define (string-has-property (o object) (c class) (property object) (flat boolean) (phase phase)) boolean
      (assert (in o string :narrow-true) (:assertion) " because " (:global string-has-property) " is only called on instances of class " (:character-literal "String") ".")
      (const qname qualified-name (object-to-qualified-name property phase))
      (const i integer-opt (multiname-to-unsigned-integer (list-set qname)))
      (cond
       ((not-in i (tag none) :narrow-true)
        (return (< i (length o))))
       (nil
        (return (or (not-in (find-base-instance-property c (list-set qname) read) (tag none))
                    (not-in (find-base-instance-property c (list-set qname) write) (tag none))
                    (not-in (find-archetype-property o (list-set qname) read flat) (tag none))
                    (not-in (find-archetype-property o (list-set qname) write flat) (tag none)))))))
    
    (define (read-string (o object) (limit class) (multiname multiname) (env environment-opt) (undefined-if-missing boolean) (phase phase))
            object-opt
      (assert (in o string :narrow-true) (:assertion) " because " (:global read-string) " is only called on instances of class " (:character-literal "String") ".")
      (when (= limit -string class)
        (const i integer-opt (multiname-to-unsigned-integer multiname))
        (when (not-in i (tag none) :narrow-true)
          (cond
           ((< i (length o)) (return (nth o i)))
           (undefined-if-missing (return undefined))
           (nil (return none)))))
      (return (ordinary-read o limit multiname env undefined-if-missing phase)))
    
    (define (construct-string (c class :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if the argument can be converted to a primitive in a constant expression.")
      (cond
       ((= (length args) 0) (return ""))
       ((= (length args) 1) (return (object-to-string (nth args 0) phase)))
       (nil (throw-error -argument-error "at most one argument can be supplied"))))
    
    (define (coerce-string (o object) (c class :unused)) (union string null (tag none))
      (cond
       ((in o (union null string) :narrow-true) (return o))
       ((in o char16 :narrow-true)
        (return (vector o)))
       (nil (return none))))
    
    
    (define (-string_length (this object) (phase phase :unused)) object
      (assert (in this string :narrow-true) (:assertion) " because this getter cannot be extracted from the " (:character-literal "String") " class.")
      (const length integer (length this))
      (return (real-to-float64 length)))
    
    
    (define (-string_from-char-code (this object :unused) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (note "This function can be used in a constant expression if the arguments can be converted to primitives in constant expressions.")
      (var s string "")
      (for-each args arg
        (const i integer (object-to-integer arg phase))
        (if (cascade integer 0 <= i <= (hex #x10FFFF))
          (<- s (append s (integer-to-u-t-f16 i)))
          (throw-error -range-error "character code out of range")))
      (return s))
    
    
    (define -string-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -string)
         (std-function (new qualified-name public "toString") -string_to-string 0)
         (std-reserve (new qualified-name public "valueOf") -object-prototype)
         (std-function (new qualified-name public "charAt") -string_char-at 1)
         (std-function (new qualified-name public "charCodeAt") -string_char-code-at 1)
         (std-function (new qualified-name public "concat") -string_concat 1)
         (std-function (new qualified-name public "indexOf") -string_index-of 1)
         (std-function (new qualified-name public "lastIndexOf") -string_last-index-of 1)
         (std-function (new qualified-name public "localeCompare") -string_locale-compare 1)
         (std-function (new qualified-name public "match") -string_match 1)
         (std-function (new qualified-name public "replace") -string_replace 1)
         (std-function (new qualified-name public "search") -string_search 1)
         (std-function (new qualified-name public "slice") -string_slice 2)
         (std-function (new qualified-name public "split") -string_split 2)
         (std-function (new qualified-name public "substring") -string_substring 2)
         (std-function (new qualified-name public "toLowerCase") -string_to-lower-case 0)
         (std-function (new qualified-name public "toLocaleLowerCase") -string_to-locale-lower-case 0)
         (std-function (new qualified-name public "toUpperCase") -string_to-upper-case 0)
         (std-function (new qualified-name public "toLocaleUpperCase") -string_to-locale-upper-case 0))
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (define (-string_to-string (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " can be converted to a primitive in a constant expression.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (return (object-to-string this phase)))
    
    
    (define (-string_char-at (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the argument can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const s string (object-to-string this phase))
      (var position extended-integer (object-to-extended-integer (default-arg args 0 +zero64) phase))
      (cond
       ((in position (tag nan) :narrow-false) (throw-error -range-error))
       ((and (not-in position (tag +infinity -infinity) :narrow-true) (cascade integer 0 <= position < (length s)))
        (return (vector (nth s position))))
       (nil (return ""))))
    
    
    (define (-string_char-code-at (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) float64
      (note "This function can be used in a constant expression if " (:local this) " and the argument can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const s string (object-to-string this phase))
      (var position extended-integer (object-to-extended-integer (default-arg args 0 +zero64) phase))
      (cond
       ((in position (tag nan) :narrow-false) (throw-error -range-error))
       ((and (not-in position (tag +infinity -infinity) :narrow-true) (cascade integer 0 <= position < (length s)))
        (return (real-to-float64 (char16-to-integer (nth s position)))))
       (nil (return nan64))))
    
    
    (define (-string_concat (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the argument can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (var s string (object-to-string this phase))
      (for-each args arg
        (<- s (append s (object-to-string arg phase))))
      (return s))
    
    
    (define (-string_index-of (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) float64
      (note "This function can be used in a constant expression if " (:local this) " and the arguments can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (set-not-in (length args) (range-set-of integer 1 2))
        (throw-error -argument-error "at least one and at most two arguments must be supplied"))
      (const s string (object-to-string this phase))
      (const pattern string (object-to-string (nth args 0) phase))
      (const arg object (default-arg args 1 +zero64))
      (var position integer (pin-extended-integer (object-to-extended-integer arg phase) (length s) false))
      (while (<= (+ position (length pattern)) (length s))
        (rwhen (= (subseq s position (+ position (- (length pattern) 1))) pattern string)
          (return (real-to-float64 position)))
        (<- position (+ position 1)))
      (return -1.0))
    
    
    (define (-string_last-index-of (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) float64
      (note "This function can be used in a constant expression if " (:local this) " and the arguments can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (set-not-in (length args) (range-set-of integer 1 2))
        (throw-error -argument-error "at least one and at most two arguments must be supplied"))
      (const s string (object-to-string this phase))
      (const pattern string (object-to-string (nth args 0) phase))
      (const arg object (default-arg args 1 +infinity64))
      (var position integer (pin-extended-integer (object-to-extended-integer arg phase) (length s) false))
      (when (> (+ position (length pattern)) (length s))
        (<- position (- (length s) (length pattern))))
      (while (>= position 0)
        (rwhen (= (subseq s position (+ position (- (length pattern) 1))) pattern string)
          (return (real-to-float64 position)))
        (<- position (- position 1)))
      (return -1.0))
    
   
    (define (-string_locale-compare (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) float64
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "localeCompare") " cannot be called from a constant expression"))
      (rwhen (< (length args) 1)
        (throw-error -argument-error "at least one argument must be supplied"))
      (const s1 string (object-to-string this phase))
      (const s2 string (object-to-string (nth args 0) phase))
      (/* "Let " (:local result) ":" :nbsp (:type object) " be a value of type " (:global -number) " that is the result of a locale-sensitive string comparison of "
          (:local s1) " and " (:local s2) ". The two strings are compared in an implementation-defined fashion. The result is intended to order strings in the sort order "
          "specified by the system default locale, and will be negative, zero, or positive, depending on whether " (:local s1) " comes before " (:local s2)
          " in the sort order, they are equal, or " (:local s1) " comes after " (:local s2) " in the sort order, respectively. The result shall not be "
          (:tag nan64) ". The comparison shall be a consistent comparison function on the set of all strings.")
      (var result float64)
      (cond
       ((< s1 s2 string) (<- result -1.0))
       ((> s1 s2 string) (<- result +1.0))
       (nil (<- result +zero64)))
      (*/)
      (return result))
    
   
    (define (-string_match (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "match") " cannot be called from a constant expression"))
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const s string (object-to-string this phase))
      (todo))
    
   
    (define (-string_replace (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "replace") " cannot be called from a constant expression"))
      (rwhen (/= (length args) 2)
        (throw-error -argument-error "exactly two arguments must be supplied"))
      (const s string (object-to-string this phase))
      (todo))
    
   
    (define (-string_search (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "search") " cannot be called from a constant expression"))
      (rwhen (/= (length args) 1)
        (throw-error -argument-error "exactly one argument must be supplied"))
      (const s string (object-to-string this phase))
      (todo))
    
    
    (define (-string_slice (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the arguments can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (> (length args) 2)
        (throw-error -argument-error "at most two arguments can be supplied"))
      (const s string (object-to-string this phase))
      (const start-arg object (default-arg args 0 +zero64))
      (const end-arg object (default-arg args 1 +infinity64))
      (const start integer (pin-extended-integer (object-to-extended-integer start-arg phase) (length s) true))
      (const end integer (pin-extended-integer (object-to-extended-integer end-arg phase) (length s) true))
      (if (< start end)
        (return (subseq s start (- end 1)))
        (return "")))
    
   
    (define (-string_split (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "split") " cannot be called from a constant expression"))
      (rwhen (> (length args) 2)
        (throw-error -argument-error "at most two arguments can be supplied"))
      (const s string (object-to-string this phase))
      (todo))
    
    
    (define (-string_substring (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " and the arguments can be converted to primitives in constant expressions.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (> (length args) 2)
        (throw-error -argument-error "at most two arguments can be supplied"))
      (const s string (object-to-string this phase))
      (const start-arg object (default-arg args 0 +zero64))
      (const end-arg object (default-arg args 1 +infinity64))
      (const start integer (pin-extended-integer (object-to-extended-integer start-arg phase) (length s) false))
      (const end integer (pin-extended-integer (object-to-extended-integer end-arg phase) (length s) false))
      (if (<= start end)
        (return (subseq s start (- end 1)))
        (return (subseq s end (- start 1)))))
    
    
    (define (-string_to-lower-case (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " can be converted to a primitive in a constant expression.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (const s string (object-to-string this phase))
      (const s32 (vector char21) (string-to-u-t-f32 s))
      (var r string "")
      (for-each s32 ch
        (<- r (append r (char-to-lower-full ch))))
      (return r))
    
    
    (define (-string_to-locale-lower-case (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "toLocaleLowerCase") " cannot be called from a constant expression"))
      (const s string (object-to-string this phase))
      (const s32 (vector char21) (string-to-u-t-f32 s))
      (var r string "")
      (for-each s32 ch
        (<- r (append r (char-to-lower-localized ch))))
      (return r))
    
    
    (define (-string_to-upper-case (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (note "This function can be used in a constant expression if " (:local this) " can be converted to a primitive in a constant expression.")
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (const s string (object-to-string this phase))
      (const s32 (vector char21) (string-to-u-t-f32 s))
      (var r string "")
      (for-each s32 ch
        (<- r (append r (char-to-upper-full ch))))
      (return r))
    
    
    (define (-string_to-locale-upper-case (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (note "This function is generic and can be applied even if " (:local this) " is not a string.")
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "toLocaleUpperCase") " cannot be called from a constant expression"))
      (const s string (object-to-string this phase))
      (const s32 (vector char21) (string-to-u-t-f32 s))
      (var r string "")
      (for-each s32 ch
        (<- r (append r (char-to-upper-localized ch))))
      (return r))
    
    
    
    (%heading (2 :semantics) "Array")
    (define -array class
      (new class
        (list-set-of local-binding)
        (%list-set-of instance-property
          (new instance-variable (list-set (new qualified-name array-private "length")) true false -number +zero64 false)
          (new instance-getter (list-set (new qualified-name public "length")) true false :uninit -array_get-length)
          (new instance-setter (list-set (new qualified-name public "length")) true false :uninit -array_set-length))
        -object (delay -array-prototype) true
        "Array" "object" array-private true true null hint-number
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read write-array ordinary-delete ordinary-enumerate
        same-as-construct ordinary-construct init-array ordinary-is ordinary-coerce))
    
    (define array-limit integer (/*/ (expt 2 53) "an implementation-defined integer value between " (:expr integer (- (expt 2 32) 1)) " and "
                                     (:expr integer (expt 2 53)) " inclusive"))
    (define array-private namespace (new namespace "private"))
    
    (define (write-array (o object) (limit class) (multiname multiname) (env environment-opt) (new-value object) (create-if-missing boolean) (phase (tag run)))
            (tag none ok)
      (const result (tag none ok) (ordinary-write o limit multiname env new-value create-if-missing phase))
      (when (in result (tag ok))
        (const i integer-opt (multiname-to-unsigned-integer multiname))
        (when (not-in i (tag none) :narrow-true)
          (rwhen (>= i array-limit)
            (throw-error -range-error "array index out of range"))
          (var length integer (read-array-private-length o phase))
          (when (>= i length)
            (<- length (+ i 1))
            (write-array-private-length o length phase))))
      (return result))
    
    (%text :comment (:global-call read-array-private-length array phase) " returns an " (:character-literal "Array") :apostrophe
           "s private length. See also " (:global read-length) ", which can work on non-" (:character-literal "Array") " objects.")
    (define (read-array-private-length (array object) (phase phase)) integer
      (const length float64 (assert-in (read-instance-slot array (new qualified-name array-private "length") phase) float64))
      (assert (not-in length (tag nan64 +infinity64 -infinity64) :narrow-true))
      (var n rational (to-rational length))
      (assert (and (in n integer :narrow-true) (cascade integer 0 <= n <= array-limit)))
      (return n))
    
    (%text :comment (:global-call write-array-private-length array length phase) " sets an " (:character-literal "Array") :apostrophe
           "s private length to " (:local length) " after ensuring that " (:local length) " is between 0 and " (:global array-limit)
           " inclusive. See also " (:global write-length) ", which can work on non-" (:character-literal "Array") " objects.")
    (define (write-array-private-length (array object) (length integer) (phase (tag run))) void
      (rwhen (or (< length 0) (> length array-limit))
        (throw-error -range-error "array length out of range"))
      (dot-write array (list-set (new qualified-name array-private "length")) (real-to-float64 length) phase))
    
    (define (multiname-to-unsigned-integer (multiname multiname)) integer-opt
      (rwhen (/= (length multiname) 1)
        (return none))
      (const qname qualified-name (unique-elt-of multiname))
      (rwhen (/= (& namespace qname) public namespace)
        (return none))
      (const name string (& id qname))
      (when (nonempty name)
        (cond
         ((= name "0" string) (return 0))
         ((and (/= (nth name 0) #\0 char16) (every name ch (set-in ch (range-set-of-ranges char16 #\0 #\9))))
          (return (assert-in (string-to-extended-integer name) integer)))))
      (return none))
    
    (define (init-array (this simple-instance) (args (vector object)) (phase (tag run))) void
      (when (= (length args) 1)
        (const arg object (nth args 0))
        (rwhen (in arg general-number :narrow-true)
          (const length integer-opt (check-integer arg))
          (rwhen (in length (tag none) :narrow-false)
            (throw-error -range-error "array length must be an integer"))
          (write-array-private-length this length phase)
          (return)))
      (var i integer 0)
      (for-each args arg
        (index-write this i arg phase)
        (<- i (+ i 1)))
      (note "The call to " (:global index-write) " above also set the array" :apostrophe "s length to " (:local i) "."))
    
    
    (define (-array_get-length (this object) (phase phase)) float64
      (assert (is this -array) (:assertion) " because this getter cannot be extracted from the " (:character-literal "Array") " class.")
      (note "An array" :apostrophe "s length is mutable, so reading it will throw " (:global -constant-error) " when " (:expr boolean (in phase (tag compile))) ".")
      (return (assert-in (read-instance-slot this (new qualified-name array-private "length") phase) float64)))
    
    (define (-array_set-length (this object) (length object) (phase phase)) void
      (assert (is this -array) (:assertion) " because this setter cannot be extracted from the " (:character-literal "Array") " class.")
      (quiet-assert (in this simple-instance :narrow-true))
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error "an array" :apostrophe "s " (:character-literal "length") " cannot be set from a constant expression"))
      (const new-length integer-opt (check-integer (object-to-general-number length phase)))
      (rwhen (or (in new-length (tag none) :narrow-false) (< new-length 0) (> new-length array-limit))
        (throw-error -range-error "array length out of range or not an integer"))
      (const old-length integer (read-array-private-length this phase))
      (when (< new-length old-length)
        (note "Delete all indexed properties greater than or equal to the new length")
        (function (qname-in-deleted-range (qname qualified-name)) boolean
          (const i integer-opt (multiname-to-unsigned-integer (list-set qname)))
          (return (and (not-in i (tag none) :narrow-true) (cascade integer new-length <= i < old-length))))
        (&= local-bindings this
            (map (& local-bindings this)
                 b b (not (qname-in-deleted-range (& qname b))))))
      (write-array-private-length this new-length phase))
    
    
    (define -array-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -array)
         (std-function (new qualified-name public "toString") -array_to-string 0)
         (std-function (new qualified-name public "toLocaleString") -array_to-locale-string 0)
         (std-function (new qualified-name public "concat") -array_concat 1)
         (std-function (new qualified-name public "join") -array_join 1)
         (std-function (new qualified-name public "pop") -array_pop 0)
         (std-function (new qualified-name public "push") -array_push 1)
         (std-function (new qualified-name public "reverse") -array_reverse 0)
         (std-function (new qualified-name public "shift") -array_shift 0)
         (std-function (new qualified-name public "slice") -array_slice 2)
         (std-function (new qualified-name public "sort") -array_sort 1)
         (std-function (new qualified-name public "splice") -array_splice 2)
         (std-function (new qualified-name public "unshift") -array_unshift 1))
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    ;***** Add some properties here
    
    
    (define (-array_to-string (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "toString") " cannot be called on an " (:character-literal "Array") " from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (return (internal-join this "," phase)))
    
    
    (define (-array_to-locale-string (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "toLocaleString") " cannot be called on an " (:character-literal "Array") " from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (note "This function passes any arguments passed to it in " (:local args) " to " (:character-literal "toLocaleString") " applied to the elements of the array.")
      (const separator string (/*/ "," "the list-separator string appropriate for the host" :apostrophe "s current locale, derived in an implementation-defined way"))
      (const length integer (read-length this phase))
      (var result string "")
      (var i integer 0)
      (while (/= i length)
        (const elt object-opt (index-read this i phase))
        (when (not-in elt (tag undefined null none) :narrow-true)
          (const to-locale-string-method object (dot-read elt (list-set (new qualified-name public "toLocaleString")) phase))
          (const s object (call elt to-locale-string-method args phase))
          (rwhen (not-in s (union char16 string) :narrow-false)
            (throw-error -type-error (:character-literal "toLocaleString") " should return a string"))
          (<- result (append result (to-string s))))
        (<- i (+ i 1))
        (when (/= i length)
          (<- result (append result separator))))
      (return result))
    
    
    (define (-array_concat (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "concat") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (const constituents (vector object) (append (vector this) args))
      (const array object (construct -array (vector-of object) phase))
      (var i integer 0)
      (for-each constituents o
        (cond
         ((is o -array)
          (const o-length integer (read-length o phase))
          (var k integer 0)
          (while (/= k o-length)
            (const elt object-opt (index-read o k phase))
            (when (not-in elt (tag none) :narrow-true)
              (index-write array i elt phase))
            (<- k (+ k 1))
            (<- i (+ i 1))))
         (nil
          (index-write array i o phase)
          (<- i (+ i 1)))))
      (write-array-private-length array i phase)
      (return array))
    
    
    (define (-array_join (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) string
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "join") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const arg object (default-arg args 0 undefined))
      (var separator string ",")
      (when (not-in arg (tag undefined))
        (<- separator (object-to-string arg phase)))
      (return (internal-join this separator phase)))
    
    
    (define (internal-join (this object) (separator string) (phase (tag run))) string
      (const length integer (read-length this phase))
      (var result string "")
      (var i integer 0)
      (while (/= i length)
        (const elt object-opt (index-read this i phase))
        (when (not-in elt (tag undefined null none) :narrow-true)
          (<- result (append result (object-to-string elt phase))))
        (<- i (+ i 1))
        (when (/= i length)
          (<- result (append result separator))))
      (return result))
    
    
    (define (-array_pop (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "pop") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (rwhen (/= (length args) 0)
        (throw-error -argument-error "no arguments can be supplied"))
      (var length integer (read-length this phase))
      (var result object undefined)
      (when (/= length 0)
        (<- length (- length 1))
        (const elt object-opt (index-read this length phase))
        (when (not-in elt (tag none) :narrow-true)
          (<- result elt)
          (index-write this length none phase)))
      (write-length this length phase)
      (return result))
    
    
    (define (-array_push (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "push") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (var length integer (read-length this phase))
      (for-each args arg
        (index-write this length arg phase)
        (<- length (+ length 1)))
      (write-length this length phase)
      (return (real-to-float64 length)))
    
    
    (define (-array_reverse (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "reverse") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (rwhen (/= (length args) 0)
        (throw-error -argument-error "no arguments can be supplied"))
      (const length integer (read-length this phase))
      (var lo integer 0)
      (var hi integer (- length 1))
      (while (< lo hi)
        (const lo-elt object-opt (index-read this lo phase))
        (const hi-elt object-opt (index-read this hi phase))
        (index-write this lo hi-elt phase)
        (index-write this hi lo-elt phase)
        (<- lo (+ lo 1))
        (<- hi (- hi 1)))
      (return this))
    
    
    (define (-array_shift (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "shift") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (rwhen (/= (length args) 0)
        (throw-error -argument-error "no arguments can be supplied"))
      (var length integer (read-length this phase))
      (var result object undefined)
      (when (/= length 0)
        (var elt object-opt (index-read this 0 phase))
        (when (not-in elt (tag none) :narrow-true)
          (<- result elt))
        (var i integer 1)
        (while (/= i length)
          (<- elt (index-read this i phase))
          (index-write this (- i 1) elt phase)
          (<- i (+ i 1)))
        (<- length (- length 1))
        (index-write this length none phase))
      (write-length this length phase)
      (return result))
    
    
    (define (-array_slice (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "slice") " cannot be called on an " (:character-literal "Array") " from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (rwhen (> (length args) 2)
        (throw-error -argument-error "at most two arguments can be supplied"))
      (const length integer (read-length this phase))
      (const start-arg object (default-arg args 0 +zero64))
      (const end-arg object (default-arg args 1 +infinity64))
      (const start integer (pin-extended-integer (object-to-extended-integer start-arg phase) length true))
      (const end integer (pin-extended-integer (object-to-extended-integer end-arg phase) length true))
      (return (make-array-slice this start end phase)))
    
    (define (make-array-slice (array object) (start integer) (end integer) (phase (tag run))) object
      (const slice object (construct -array (vector-of object) phase))
      (var i integer start)
      (var j integer 0)
      (while (< i end)
        (const elt object-opt (index-read array i phase))
        (index-write slice j elt phase)
        (<- i (+ i 1))
        (<- j (+ j 1)))
      (write-length slice j phase)
      (return slice))

    
    (define (-array_sort (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "sort") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (todo))
    
    
    (define (-array_splice (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) object
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "splice") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (rwhen (< (length args) 2)
        (throw-error -argument-error "at least two arguments must be supplied"))
      (const length integer (read-length this phase))
      (const start-arg object (default-arg args 0 +zero64))
      (const delete-count-arg object (default-arg args 1 +zero64))
      (const start integer (pin-extended-integer (object-to-extended-integer start-arg phase) length true))
      (const delete-count integer (pin-extended-integer (object-to-extended-integer delete-count-arg phase) (- length start) false))
      (const deleted-slice object (make-array-slice this start (+ start delete-count) phase))
      (const new-elts (vector object) (subseq args 2))
      (const new-elt-count integer (length new-elts))
      (const count-diff integer (- new-elt-count delete-count))
      (var i integer)
      (cond
       ((< count-diff 0)
        (<- i (+ start delete-count))
        (while (/= i length)
          (const elt object-opt (index-read this i phase))
          (index-write this (+ i count-diff) elt phase)
          (<- i (+ i 1)))
        (<- i 0)
        (while (/= i count-diff)
          (<- i (- i 1))
          (index-write this (+ length i) none phase)))
       ((> count-diff 0)
        (<- i length)
        (while (/= i (+ start delete-count))
          (<- i (- i 1))
          (const elt object-opt (index-read this i phase))
          (index-write this (+ i count-diff) elt phase))))
      (write-length this (+ length count-diff) phase)
      (<- i start)
      (for-each new-elts arg
        (index-write this i arg phase)
        (<- i (+ i 1)))
      (return deleted-slice))
    
    
    (define (-array_unshift (this object) (f simple-instance :unused) (args (vector object)) (phase phase)) float64
      (rwhen (in phase (tag compile) :narrow-false)
        (throw-error -constant-error (:character-literal "unshift") " cannot be called from a constant expression"))
      (note "This function is generic and can be applied even if " (:local this) " is not an " (:character-literal "Array") ".")
      (var i integer (read-length this phase))
      (const n-args integer (length args))
      (const new-length integer (+ n-args i))
      (when (= n-args 0)
        (// "At the implementation" :apostrophe "s discretion, either do nothing or " (:keyword return) :nbsp (:expr float64 (real-to-float64 new-length))))
      (write-length this new-length phase)
      (while (/= i 0)
        (<- i (- i 1))
        (const elt object-opt (index-read this i phase))
        (index-write this (+ i n-args) elt phase))
      (for-each args arg
        (index-write this i arg phase)
        (<- i (+ i 1)))
      (return (real-to-float64 new-length)))
    
    
    
    (%heading (2 :semantics) "Namespace")
    (define -namespace class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object (delay -namespace-prototype) true
        "Namespace" "namespace" :uninit false true null hint-string
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        ordinary-call construct-namespace none ordinary-is ordinary-coerce))
    
    (define (construct-namespace (c class :unused) (args (vector object)) (phase phase)) namespace
      (note "This function can be used in a constant expression if its argument is a string.")
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const arg object (default-arg args 0 undefined))
      (cond
       ((in arg (union null undefined))
        (rwhen (in phase (tag compile))
          (throw-error -constant-error "a constant expression cannot construct new anonymous namespaces"))
        (return (new namespace "anonymous")))
       ((in arg (union char16 string) :narrow-true)
        (const name string (to-string arg))
        (reserve ns)
        (cond
         ((= name "" string)
          (return public))
         ((some named-namespaces ns (= (& name ns) name string) :define-true)
          (return ns))
         (nil
          (const ns2 namespace (new namespace name))
          (<- named-namespaces (set+ named-namespaces (list-set ns2)))
          (return ns2))))
       (nil (throw-error -type-error))))
    
    (defvar named-namespaces (list-set namespace) (list-set-of namespace))
    
    (define -namespace-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-function (new qualified-name public "toString") -namespace_to-string 0)
         (std-reserve (new qualified-name public "valueOf") -object-prototype))
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (define (-namespace_to-string (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) string
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (rwhen (not-in this namespace :narrow-false)
        (throw-error -type-error))
      (return (& name this)))
    
    
    (%heading (2 :semantics) "Attribute")
    (define -attribute class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object (delay -object-prototype) true
        "Attribute" "object" :uninit false true null hint-string
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        dummy-call dummy-construct none ordinary-is ordinary-coerce))
    
    
    (%heading (2 :semantics) "Date")
    (define -date class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object (delay -date-prototype) true
        "Date" "object" :uninit true true null hint-string
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        dummy-call dummy-construct none ordinary-is ordinary-coerce))
    
    (define -date-prototype simple-instance
      (new simple-instance
        (list-set-of local-binding)
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    ;***** Add some properties here
    
    
    (%heading (2 :semantics) "RegExp")
    (define -reg-exp class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object (delay -reg-exp-prototype) true
        "RegExp" "object" :uninit true true null hint-number
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        dummy-call dummy-construct none ordinary-is ordinary-coerce))
    
    (define -reg-exp-prototype simple-instance
      (new simple-instance
        (list-set-of local-binding)
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    ;***** Add some properties here
    
    
    (%heading (2 :semantics) "Class")
    (define -class class
      (new class
        (list-set-of local-binding)
        (list-set-of instance-property class-prototype-getter)
        -object (delay -class-prototype) true
        "Class" "function" :uninit false true null hint-string
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        dummy-call dummy-construct none ordinary-is ordinary-coerce))
    
    (define class-prototype-getter instance-getter (new instance-getter (list-set (new qualified-name public "prototype")) true false :uninit -class_prototype))
    (define (-class_prototype (this object) (phase phase :unused)) object
      (assert (in this class :narrow-true) (:assertion) " because this getter cannot be extracted from the " (:character-literal "Class") " class.")
      (const prototype object-opt (&opt prototype this))
      (if (in prototype (tag none) :narrow-false)
        (return undefined)
        (return prototype)))
    
    (define -class-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -class)
         (std-function (new qualified-name public "toString") -class_to-string 0)
         (std-reserve (new qualified-name public "valueOf") -object-prototype)
         (std-const-binding (new qualified-name public "length") -number 1.0))
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (define (-class_to-string (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase :unused)) string
      (note "This function does not check " (:local phase) " and therefore can be used in a constant expression.")
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (const c class (object-to-class this))
      (return (append "[class " (& name c) "]")))
    
    
    (%heading (2 :semantics) "Function")
    (define -function class
      (new class
        (list-set-of local-binding)
        (list-set-of instance-property ivar-function-length)
        -object (delay -function-prototype) true
        "Function" "function" :uninit false true null hint-string
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        dummy-call dummy-construct none ordinary-is ordinary-coerce))
    
    (define ivar-function-length instance-variable (new instance-variable (list-set (new qualified-name public "length")) true false -number none true))
    
    (define -function-prototype simple-instance
      (new simple-instance
        (list-set-of local-binding)
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    ;***** Add some properties here
    
    
    (%heading (3 :semantics) "PrototypeFunction")
    (define -prototype-function class
      (new class
        (list-set-of local-binding)
        (list-set-of instance-property
          (new instance-variable (list-set (new qualified-name public "prototype")) true false -object undefined false))
        -function (delay -function-prototype) true
        "Function" "function" :uninit true true null hint-string
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        dummy-call dummy-construct none ordinary-is ordinary-coerce)) ;***** Need to set prototype here.
    
    
    (%heading (2 :semantics) "Package")
    (define -package class
      (new class
        (list-set-of local-binding) (list-set-of instance-property) -object (delay -object-prototype) true
        "Package" "object" :uninit true true null hint-string
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        dummy-call dummy-construct none ordinary-is ordinary-coerce))
    
    
    (%heading (2 :semantics) "Error")
    (define -error class
      (new class
        (list-set-of local-binding)
        (%list-set-of instance-property
          (new instance-variable (list-set (new qualified-name public "name")) false true -string null false)
          (new instance-variable (list-set (new qualified-name public "message")) false true -string null false))
        -object (delay -error-prototype) true
        "Error" "object" :uninit true false null hint-number
        ordinary-has-property ordinary-bracket-read ordinary-bracket-write ordinary-bracket-delete ordinary-read ordinary-write ordinary-delete ordinary-enumerate
        call-error ordinary-construct init-error ordinary-is ordinary-coerce))
    
    (define (call-error (this object :unused) (c class) (args (vector object)) (phase phase)) object
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const arg object (default-arg args 0 undefined))
      (if (or (in arg (tag null)) (is arg -error))
        (return arg)
        (return (construct c args phase))))
    
    (define (init-error (this simple-instance) (args (vector object)) (phase (tag run))) void
      (rwhen (> (length args) 1)
        (throw-error -argument-error "at most one argument can be supplied"))
      (const name (union string null) (assert-in (dot-read -error-prototype (list-set (new qualified-name public "name")) phase) (union string null)))
      (dot-write this (list-set (new qualified-name public "name")) name phase)
      (const arg object (default-arg args 0 undefined))
      (var message (union string null))
      (if (in arg (tag undefined))
        (<- message (assert-in (dot-read -error-prototype (list-set (new qualified-name public "message")) phase) (union string null)))
        (<- message (object-to-string arg phase)))
      (dot-write this (list-set (new qualified-name public "message")) message phase))
    
    
    (define -error-prototype simple-instance
      (new simple-instance
        (%list-set
         (std-const-binding (new qualified-name public "constructor") -class -error)
         (std-function (new qualified-name public "toString") -error_to-string 1)
         (std-var-binding (new qualified-name public "name") -string "Error")
         (std-var-binding (new qualified-name public "message") -string (/*/ "ErrorPrototypeMessage" "an implementation-defined string")))
        -object-prototype prototypes-sealed -object
        (list-set-of slot) none none none))
    
    
    (define (-error_to-string (this object) (f simple-instance :unused) (args (vector object) :unused) (phase phase)) string
      (rwhen (in phase (tag compile))
        (throw-error -constant-error (:character-literal "toString") " cannot be called on an " (:character-literal "Error") " from a constant expression"))
      (note "This function ignores any arguments passed to it in " (:local args) ".")
      (const err object (coerce-non-null this -error))
      (const name (union string null) (assert-in (dot-read err (list-set (new qualified-name public "name")) phase) (union string null)))
      (const message (union string null) (assert-in (dot-read err (list-set (new qualified-name public "message")) phase) (union string null)))
      (return (/*/ (append (object-to-string name phase) ": " (object-to-string message phase))
                "an implementation-defined string derived from " (:local name) ", " (:local message) ", and optionally other properties of " (:local err))))
    
    
    
    ;***** Describe what I do.
    (define (system-error (e class) (msg (union string undefined))) object
      (return (construct e (vector-of object msg) run)))
    
    
    (%heading (3 :semantics) "Error Subclasses")
    (define (make-built-in-error-subclass (name string)) class
      (function (call (this object :unused) (c class) (args (vector object)) (phase phase)) object
        (rwhen (> (length args) 1)
          (throw-error -argument-error "at most one argument can be supplied"))
        (const arg object (default-arg args 0 undefined))
        (if (or (in arg (tag null)) (is arg -error))
          (return (coerce arg c))
          (return (construct c args phase))))
      (const c class
        (new class
          (list-set-of local-binding) (list-set-of instance-property) -error :uninit false
          name "object" :uninit true false null hint-number
          (& has-property -error) (& bracket-read -error) (& bracket-write -error) (& bracket-delete -error)
          (& read -error) (& write -error) (& delete -error)
          (& enumerate -error)
          call ordinary-construct none ordinary-is ordinary-coerce))
      (const prototype simple-instance
        (new simple-instance
          (%list-set
           (std-const-binding (new qualified-name public "constructor") -class c)
           (std-var-binding (new qualified-name public "name") -string name)
           (std-var-binding (new qualified-name public "message") -string (/*/ (append name "PrototypeMessage") "an implementation-defined string")))
          -error-prototype prototypes-sealed -object
          (list-set-of slot) none none none))
      (function (init (this simple-instance) (args (vector object)) (phase (tag run))) void
        (rwhen (> (length args) 1)
          (throw-error -argument-error "at most one argument can be supplied"))
        (const name2 (union string null) (assert-in (dot-read prototype (list-set (new qualified-name public "name")) phase) (union string null)))
        (dot-write this (list-set (new qualified-name public "name")) name2 phase)
        (const arg object (default-arg args 0 undefined))
        (var message (union string null))
        (if (in arg (tag undefined))
          (<- message (assert-in (dot-read prototype (list-set (new qualified-name public "message")) phase) (union string null)))
          (<- message (object-to-string arg phase)))
        (dot-write this (list-set (new qualified-name public "message")) message phase))
      (&const= prototype c prototype)
      (&= init c init)
      (&= complete c true)
      (return c))
    
    
    (define -argument-error class (make-built-in-error-subclass "ArgumentError"))
    (define -attribute-error class (make-built-in-error-subclass "AttributeError"))
    (define -constant-error class (make-built-in-error-subclass "ConstantError"))
    (define -definition-error class (make-built-in-error-subclass "DefinitionError"))
    (define -eval-error class (make-built-in-error-subclass "EvalError"))
    (define -range-error class (make-built-in-error-subclass "RangeError"))
    (define -reference-error class (make-built-in-error-subclass "ReferenceError"))
    (define -syntax-error class (make-built-in-error-subclass "SyntaxError"))
    (define -type-error class (make-built-in-error-subclass "TypeError"))
    (define -uninitialized-error class (make-built-in-error-subclass "UninitializedError"))
    (define -u-r-i-error class (make-built-in-error-subclass "URIError"))
    
    
    (? hide
      (%heading (1 :semantics) "Pretty Printing")
      
      (define (object-to-source (o object)) string
        (case o
          (:narrow (union undefined null boolean) (return (object-to-string o run)))
          (:narrow long (return (append (general-number-to-string o) "L")))
          (:narrow u-long (return (append (general-number-to-string o) "UL")))
          (:select (tag nan32) (return "NaN_f"))
          (:select (tag +zero32) (return "+0.0f"))
          (:select (tag -zero32) (return "-0.0f"))
          (:select (tag +infinity32) (return "Infinity_f"))
          (:select (tag -infinity32) (return "-Infinity_f"))
          (:narrow nonzero-finite-float32 (return (append (general-number-to-string o) "f")))
          (:select (tag nan64) (return "NaN"))
          (:select (tag +zero64) (return "+0.0"))
          (:select (tag -zero64) (return "-0.0"))
          (:select (tag +infinity64) (return "Infinity"))
          (:select (tag -infinity64) (return "-Infinity"))
          (:narrow nonzero-finite-float64 (return (general-number-to-string o)))
          (:narrow char16 (return (vector #\' o #\')))
          (:narrow string (return (append "\"" o "\"")))
          (:narrow namespace (return (append "Namespace \"" (object-to-string o run) "\"")))
          (:narrow compound-attribute (return (append "CompoundAttribute \"" (object-to-string o run) "\"")))
          (:narrow class (return (append "Class " (& name o))))
          (:narrow simple-instance (return (append "SimpleInstance \"" (object-to-string o run) "\"")))
          (:narrow method-closure (return "MethodClosure"))
          (:narrow date (return (append "Date \"" (object-to-string o run) "\"")))
          (:narrow reg-exp (return (append "RegExp \"" (object-to-string o run) "\"")))
          (:narrow package (return (append "Package \"" (object-to-string o run) "\""))))))
    
    ))


(defparameter *jw* (generate-world "J" *jw-source* '((js2 . :js2) (es4 . :es4) (hide . :hide))))
(defparameter *jg* (world-grammar *jw* 'code-grammar))
(ensure-lf-subset *jg*)
(forward-parser-states *jg*)
#+allegro (clean-grammar *jg*) ;Remove this line if you wish to print the grammar's state tables.

(defparameter *ew* nil)
(defparameter *eg* nil)

(defun compute-ecma-subset ()
  (unless *ew*
    (setq *ew* (generate-world "E" *jw-source* '((js2 . delete) (es4 . nil) (hide . :hide))))
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
      (setf (svref bins 4) (list 'abstract 'class 'const 'debugger 'enum 'export 'extends 'goto 'implements 'import
                                 'instanceof 'interface 'native 'package 'private 'protected 'public 'super 'synchronized
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
