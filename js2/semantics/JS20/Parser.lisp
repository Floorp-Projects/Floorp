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
    (deftag method-not-found-error)
    (deftype semantic-error (tag syntax-error reference-error type-error method-not-found-error))
    
    (deftag go-break (value object) (label string))
    (deftag go-continue (value object) (label string))
    (deftag go-return (value object))
    (deftag go-throw (value object))
    (deftype early-exit (tag go-break go-continue go-return go-throw))
    
    (deftype semantic-exception (union early-exit semantic-error))
    
    
    (%subsection :semantics "Undefined")
    (deftag undefined)
    (deftype undefined (tag undefined))
    
    
    (%subsection :semantics "Null")
    (deftag null)
    (deftype null (tag null))
    
    
    (%subsection :semantics "Namespaces")
    (defrecord namespace)
    (deftype namespace (tag namespace))
    
    (define public-namespace namespace (tag namespace))
    
    
    (%subsection :semantics "Classes")
    (defrecord class
      (superclass class-opt)
      (globals (vector property) :var)
      (prototype object :var)
      (primitive boolean))
    (deftype class (tag class))
    (deftype class-opt (union null class))
    
    (define object-class class (tag class null (vector-of property) null true))
    (define undefined-class class (tag class object-class (vector-of property) null true))
    (define null-class class (tag class object-class (vector-of property) null true))
    (define boolean-class class (tag class object-class (vector-of property) null true))
    (define number-class class (tag class object-class (vector-of property) null true))
    (define string-class class (tag class object-class (vector-of property) null false))
    (define namespace-class class (tag class object-class (vector-of property) null false))
    (define class-class class (tag class object-class (vector-of property) null false))
    
    (%text :comment "Return " (:tag true) " if " (:local c) " is " (:local d) " or a subclass of " (:local d) ".")
    (define (is-subclass (c class) (d class)) boolean
      (cond
       ((= c d class) (return true))
       (nil (const b class-opt (& superclass c))
            (rwhen (:narrow-false (in (tag null) b))
              (return false))
            (return (is-subclass b d)))))
    
    (%text :comment "Return " (:tag true) " if " (:local c) " is a subclass of " (:local d) " other than " (:local d) " itself.")
    (define (is-proper-subclass (c class) (d class)) boolean
      (const b class-opt (& superclass c))
      (rwhen (:narrow-false (in (tag null) b))
        (return false))
      (return (is-subclass b d)))
    
    
    (%subsection :semantics "Structures")
    (defrecord structure
      (type class)
      (slots (vector slot) :var)
      (properties (vector property) :var))
    (deftype structure (tag structure))
    
    
    (%subsection :semantics "Objects")
    
    (deftype object (union undefined null boolean float64 string namespace class structure))
    
    
    (%text :comment "Return " (:local o) :apostrophe "s most specific type.")
    (define (object-type (o object)) class
      (case o
        (:select undefined (return undefined-class))
        (:select null (return null-class))
        (:select boolean (return boolean-class))
        (:select float64 (return number-class))
        (:select string (return string-class))
        (:select namespace (return namespace-class))
        (:select class (return class-class))
        (:narrow structure (return (& type o)))))
    
    (%text :comment "Return " (:tag true) " if " (:local o) " is an instance of class " (:local c) ". Consider "
           (:tag null) " to be an instance of the classes " (:character-literal "Null") " and "
           (:character-literal "Object") " only.")
    (define (instance-of (o object) (c class)) boolean
      (return (is-subclass (object-type o) c)))
    
    (%text :comment "Return " (:tag true) " if " (:local o) " is an instance of class " (:local c) ". Consider "
           (:tag null) " to be an instance of the classes " (:character-literal "Null") ", "
           (:character-literal "Object") ", and all other non-primitive classes.")
    (define (member-of (o object) (c class)) boolean
      (const t class (object-type o))
      (return (or (is-subclass t c)
                  (and (= o null object) (not (& primitive c))))))
    
    (define (to-boolean (o object)) boolean
      (case o
        (:select (union undefined null) (return false))
        (:narrow boolean (return o))
        (:narrow float64 (return (not-in (tag +zero -zero nan) o)))
        (:narrow string (return (/= o "" string)))
        (:select (union namespace class) (return true))
        (:select structure (todo))))
    
    (define (to-number (o object)) float64
      (case o
        (:select undefined (return nan))
        (:select (union null false) (return +zero))
        (:select true (return 1.0))
        (:narrow float64 (return o))
        (:select string (todo))
        (:select (union namespace class) (throw type-error))
        (:select structure (todo))))
    
    (define (to-string (o object)) string
      (case o
        (:select undefined (return "undefined"))
        (:select null (return "null"))
        (:select false (return "false"))
        (:select true (return "true"))
        (:select float64 (todo))
        (:narrow string (return o))
        (:select namespace (todo))
        (:select class (todo))
        (:select structure (todo))))
    
    (define (to-primitive (o object) (hint object :unused)) object
      (case o
        (:select (union undefined null boolean float64 string) (return o))
        (:select (union namespace class structure) (return (to-string o)))))
    
    (define (u-int32-to-int32 (i integer)) integer
      (if (< i (expt 2 31))
        (return i)
        (return (- i (expt 2 32)))))
    
    (define (to-u-int32 (x float64)) integer
      (rwhen (:narrow-false (in (tag +infinity -infinity nan) x))
        (return 0))
      (return (mod (truncate-float64 x) (expt 2 32))))
    
    (define (to-int32 (x float64)) integer
      (return (u-int32-to-int32 (to-u-int32 x))))
    
    (define (float64-equal (x float64) (y float64)) boolean
      (return (float64-compare x y false true false false)))
    
    
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
    (defrecord slot-id)
    (deftype slot-id (tag slot-id))
    
    (defrecord slot
      (id slot-id)
      (type class)
      (value object :var))
    (deftype slot (tag slot))
    
    (define (find-slot (id slot-id) (slots (vector slot))) slot
      (const matching-slots (vector slot)
        (map slots s s (= (& id s) id slot-id)))
      (assert (= (length matching-slots) 1))
      (return (nth matching-slots 0)))
    
    
    (%subsection :semantics "Properties")
    (deftag property 
      (name qualified-name)
      (getter accessor-info)
      (setter accessor-info)
      (fixed boolean)
      (enumerable boolean)
      (deletable boolean))
    (deftype property (tag property))
    
    (deftag qualified-name (namespace namespace) (name string))
    (deftype qualified-name (tag qualified-name))
    
    (define (find-property (n qualified-name) (properties (vector property))) (vector property)
      (return (map properties p p (= n (& name p) qualified-name))))
    
    
    (%subsection :semantics "Accessors")
    (deftag inaccessible-accessor)
    (deftag abstract-accessor)
    (deftag constant-accessor (value object))
    (deftag slot-accessor (id slot-id))
    (deftag indirect-accessor (f object))
    (deftag alias-accessor (name qualified-name))
    
    (deftype accessor (tag inaccessible-accessor abstract-accessor constant-accessor slot-accessor indirect-accessor alias-accessor))
    
    (deftag accessor-info 
      (self-type class)
      (final boolean)
      (accessor accessor))
    (deftype accessor-info (tag accessor-info))
    
    
    (%subsection :semantics "Binary Operators")
    (deftag bin-op-method
      (left-type class)
      (right-type class)
      (op (-> (object object) object)))
    (deftype bin-op-method (tag bin-op-method))
    
    (defrecord binary-operator
      (methods (vector bin-op-method) :var))
    (deftype binary-operator (tag binary-operator))
    
    
    (%text :comment "Return " (:tag true) " if " (:local m1) " is at least as specific as " (:local m2) ".")
    (define (is-bin-op-subclass (m1 bin-op-method) (m2 bin-op-method)) boolean
      (return (and (is-subclass (& left-type m1) (& left-type m2))
                   (is-subclass (& right-type m1) (& right-type m2)))))
    
    (define (limited-instance-of (v object) (c class) (limit class-opt)) boolean
      (if (instance-of v c)
        (if (:narrow-false (in (tag null) limit))
          (return true)
          (return (is-proper-subclass limit c)))
        (return false)))
    
    (%text :comment "Return a function that takes two " (:type object) " arguments " (:local left) " and " (:local right)
           " and returns the operator " (:local bin-op) " applied to " (:local left) " and " (:local right)
           ". If " (:local left-limit) " is a " (:type class)
           ", restrict the lookup to operator definitions with a superclass of " (:local left-limit)
           " for the left operand. Similarly, if " (:local right-limit) " is a " (:type class)
           ", restrict the lookup to operator definitions with a superclass of " (:local right-limit) " for the right operand.")
    (define (bin-op-eval (bin-op binary-operator) (left-limit class-opt) (right-limit class-opt)) (-> (object object) object)
      (function (f (left object) (right object)) object
        (const applicable-ops (vector bin-op-method)
          (map (& methods bin-op) m m (and (limited-instance-of left (& left-type m) left-limit)
                                           (limited-instance-of right (& right-type m) right-limit))))
        (const best-ops (vector bin-op-method)
          (map applicable-ops m m
               (empty (map applicable-ops m2 m2 (not (is-bin-op-subclass m m2))))))
        (rwhen (empty best-ops)
          (throw method-not-found-error))
        (assert (= (length best-ops) 1))
        (return ((& op (nth best-ops 0)) left right)))
      (return f))
    
    
    (%subsection :semantics "Binary Operator Tables")
    
    (define (add-objects (a object) (b object)) object
      (const ap object (to-primitive a null))
      (const bp object (to-primitive b null))
      (if (or (in string ap) (in string bp))
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
      (if (:narrow-true (and (in string ap) (in string bp)))
        (return (< ap bp string))
        (return (float64-compare (to-number ap) (to-number bp) true false false false))))
    
    (define (less-or-equal-objects (a object) (b object)) object
      (const ap object (to-primitive a null))
      (const bp object (to-primitive b null))
      (if (:narrow-true (and (in string ap) (in string bp)))
        (return (<= ap bp string))
        (return (float64-compare (to-number ap) (to-number bp) true true false false))))
    
    (define (equal-objects (a object) (b object)) object
      (case a
        (:select (union undefined null)
          (return (in (union undefined null) b)))
        (:narrow boolean
          (if (:narrow-true (in boolean b))
            (return (= a b boolean))
            (return (equal-objects (to-number a) b))))
        (:narrow float64
          (const bp object (to-primitive b null))
          (case bp
            (:select (union undefined null namespace class structure) (return false))
            (:select (union boolean string float64) (return (float64-equal a (to-number bp))))))
        (:narrow string
          (const bp object (to-primitive b null))
          (case bp
            (:select (union undefined null namespace class structure) (return false))
            (:select (union boolean float64) (return (float64-equal (to-number a) (to-number bp))))
            (:narrow string (return (= a bp string)))))
        (:select (union namespace class structure)
          (case b
            (:select (union undefined null) (return false))
            (:select (union namespace class structure) (return (strict-equal-objects a b)))
            (:select (union boolean float64 string)
              (const ap object (to-primitive a null))
              (case ap
                (:select (union undefined null namespace class structure) (return false))
                (:select (union boolean float64 string) (return (equal-objects ap b)))))))))
    
    (define (strict-equal-objects (a object) (b object)) object
      (if (:narrow-true (and (in float64 a) (in float64 b)))
        (return (float64-equal a b))
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
    
    
    (define bin-op-add binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class add-objects))))
    (define bin-op-subtract binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class subtract-objects))))
    (define bin-op-multiply binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class multiply-objects))))
    (define bin-op-divide binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class divide-objects))))
    (define bin-op-remainder binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class remainder-objects))))
    (define bin-op-less binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class less-objects))))
    (define bin-op-less-or-equal binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class less-or-equal-objects))))
    (define bin-op-equal binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class equal-objects))))
    (define bin-op-strict-equal binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class strict-equal-objects))))
    (define bin-op-shift-left binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class shift-left-objects))))
    (define bin-op-shift-right binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class shift-right-objects))))
    (define bin-op-shift-right-unsigned binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class shift-right-unsigned-objects))))
    (define bin-op-bitwise-and binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class bitwise-and-objects))))
    (define bin-op-bitwise-xor binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class bitwise-xor-objects))))
    (define bin-op-bitwise-or binary-operator (tag binary-operator (vector (tag bin-op-method object-class object-class bitwise-or-objects))))
    
    
    (%subsection :semantics "Unary Operators")
    (deftag unary-operator 
      (operand-type class)
      (op (-> (object) object)))
    (deftype unary-operator (tag unary-operator))
    
    (define (unary-not (a object)) object
      (return (not (to-boolean a))))
    
    
    (%subsection :semantics "Environments")
    (deftag static-env
      (enclosing-class class-opt)
      (labels (vector string))
      (can-return boolean)
      (constants (vector definition)))
    (deftype static-env (tag static-env))
    
    (define initial-static-env static-env (tag static-env null (vector-of string) false (vector-of definition)))
    
    (%text :comment "Return a " (:type static-env) " with label " (:local label) " prepended to " (:local s) ".")
    (define (add-label (t static-env) (label string)) static-env
      (return (tag static-env (& enclosing-class t) (append (vector label) (& labels t)) (& can-return t) (& constants t))))
    
    (%text :comment "Return " (:tag true) " if " (:character-literal "super") " is permitted here.")
    (define (allows-super (s static-env)) boolean
      (return (/= (& enclosing-class s) null class-opt)))
    
    
    (deftag dynamic-env
      (parent dynamic-env-opt)
      (definitions (vector definition)))
    (deftype dynamic-env (tag dynamic-env))
    (deftype dynamic-env-opt (union null dynamic-env))
    
    (%text :comment "If the " (:type dynamic-env) " is from within a class" :apostrophe "s body, return that class; otherwise, return " (:tag null) ".")
    (define (lexical-class (e dynamic-env :unused)) class-opt
      (todo))
    
    (define initial-dynamic-env dynamic-env (tag dynamic-env null (vector-of definition)))
    
    (deftag definition 
      (name qualified-name)
      (getter accessor-info)
      (setter accessor-info))
    (deftype definition (tag definition))
    
    
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
      (production :identifier (include) identifier-include (name "include")))
    
    (production :qualifier (:identifier) qualifier-identifier)
    (production :qualifier (public) qualifier-public)
    (production :qualifier (private) qualifier-private)
    ;(production :qualifier (:qualifier \:\: :identifier) qualifier-identifier-qualifier)
    
    (production :simple-qualified-identifier (:identifier) simple-qualified-identifier-identifier)
    (production :simple-qualified-identifier (:qualifier \:\: :identifier) simple-qualified-identifier-qualifier)
    
    (production :expression-qualified-identifier (:parenthesized-expression \:\: :identifier) expression-qualified-identifier-identifier)
    
    (production :qualified-identifier (:simple-qualified-identifier) qualified-identifier-simple)
    (production :qualified-identifier (:expression-qualified-identifier) qualified-identifier-expression)
    
    
    (%subsection "Unit Expressions")
    (production :unit-expression (:parenthesized-list-expression) unit-expression-parenthesized-list-expression)
    (production :unit-expression ($number :no-line-break $string) unit-expression-number-with-unit)
    (production :unit-expression (:unit-expression :no-line-break $string) unit-expression-unit-expression-with-unit)
    
    (%subsection "Primary Expressions")
    (production :primary-expression (null) primary-expression-null)
    (production :primary-expression (true) primary-expression-true)
    (production :primary-expression (false) primary-expression-false)
    (production :primary-expression (public) primary-expression-public)
    (production :primary-expression ($number) primary-expression-number)
    (production :primary-expression ($string) primary-expression-string)
    (production :primary-expression (this) primary-expression-this)
    (production :primary-expression ($regular-expression) primary-expression-regular-expression)
    (production :primary-expression (:unit-expression) primary-expression-unit-expression)
    (production :primary-expression (:array-literal) primary-expression-array-literal)
    (production :primary-expression (:object-literal) primary-expression-object-literal)
    (production :primary-expression (:function-expression) primary-expression-function-expression)
    (%print-actions)
    
    (production :parenthesized-expression (\( (:assignment-expression allow-in) \)) parenthesized-expression-assignment-expression)
    
    (rule :parenthesized-list-expression ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
      (production :parenthesized-list-expression (:parenthesized-expression) parenthesized-list-expression-parenthesized-expression
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :parenthesized-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) parenthesized-list-expression-list-expression
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo))))
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
    
    
    (%subsection "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    
    
    (%subsection "Super Operator")
    (rule :super-expression ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :super-expression (super) super-expression-super
        ((verify s)
         (rwhen (not (allows-super s))
           (throw syntax-error)))
        ((eval (e :unused)) (todo))
        ((super e) (return (lexical-class e))))
      (production :super-expression (:full-super-expression) super-expression-full-super-expression
        ((verify s)
         (rwhen (not (allows-super s))
           (throw syntax-error))
         (todo))
        ((eval (e :unused)) (todo))
        ((super e) (return (lexical-class e)))))
    
    (production :full-super-expression (super :parenthesized-expression) full-super-expression-super-parenthesized-expression)
    (%print-actions)
    
    
    (%subsection "Postfix Operators")
    (rule :postfix-expression ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
    
    (rule :postfix-expression-or-super ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :postfix-expression-or-super (:postfix-expression) postfix-expression-or-super-postfix-expression
        (verify (verify :postfix-expression))
        (eval (eval :postfix-expression))
        ((super (e :unused)) (return null)))
      (production :postfix-expression-or-super (:super-expression) postfix-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    (production :attribute-expression (:simple-qualified-identifier) attribute-expression-simple-qualified-identifier)
    (production :attribute-expression (:attribute-expression :member-operator) attribute-expression-member-operator)
    (production :attribute-expression (:attribute-expression :arguments) attribute-expression-call)
    
    (production :full-postfix-expression (:primary-expression) full-postfix-expression-primary-expression)
    (production :full-postfix-expression (:expression-qualified-identifier) full-postfix-expression-expression-qualified-identifier)
    (production :full-postfix-expression (:full-new-expression) full-postfix-expression-full-new-expression)
    (production :full-postfix-expression (:full-postfix-expression :member-operator) full-postfix-expression-member-operator)
    (production :full-postfix-expression (:super-expression :dot-operator) full-postfix-expression-super-dot-operator)
    (production :full-postfix-expression (:full-postfix-expression :arguments) full-postfix-expression-call)
    (production :full-postfix-expression (:full-super-expression :arguments) full-postfix-expression-super-call)
    (production :full-postfix-expression (:postfix-expression-or-super :no-line-break ++) full-postfix-expression-increment)
    (production :full-postfix-expression (:postfix-expression-or-super :no-line-break --) full-postfix-expression-decrement)
    
    (production :full-new-expression (new :full-new-subexpression :arguments) full-new-expression-new)
    (production :full-new-expression (new :full-super-expression :arguments) full-new-expression-super-new)
    
    (production :full-new-subexpression (:primary-expression) full-new-subexpression-primary-expression)
    (production :full-new-subexpression (:qualified-identifier) full-new-subexpression-qualified-identifier)
    (production :full-new-subexpression (:full-new-expression) full-new-subexpression-full-new-expression)
    (production :full-new-subexpression (:full-new-subexpression :member-operator) full-new-subexpression-member-operator)
    (production :full-new-subexpression (:super-expression :dot-operator) full-new-subexpression-super-dot-operator)
    
    (production :short-new-expression (new :short-new-subexpression) short-new-expression-new)
    (production :short-new-expression (new :super-expression) short-new-expression-super-new)
    
    (production :short-new-subexpression (:full-new-subexpression) short-new-subexpression-new-full)
    (production :short-new-subexpression (:short-new-expression) short-new-subexpression-new-short)
    
    
    (production :member-operator (:dot-operator) member-operator-dot-operator)
    (production :member-operator (\. class) member-operator-class)
    (production :member-operator (\. :parenthesized-expression) member-operator-indirect)
    
    (production :dot-operator (\. :qualified-identifier) dot-operator-qualified-identifier)
    (production :dot-operator (:brackets) dot-operator-brackets)
    
    (production :brackets ([ ]) brackets-none)
    (production :brackets ([ (:list-expression allow-in) ]) brackets-unnamed)
    (production :brackets ([ :named-argument-list ]) brackets-named)
    
    (production :arguments (:parenthesized-expressions) arguments-parenthesized-expressions)
    (production :arguments (\( :named-argument-list \)) arguments-named)
    
    (production :parenthesized-expressions (\( \)) parenthesized-expressions-none)
    (production :parenthesized-expressions (:parenthesized-list-expression) parenthesized-expressions-some)
    
    (production :named-argument-list (:literal-field) named-argument-list-one)
    (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed)
    (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more)
    
    
    (%subsection "Unary Operators")
    (production :unary-expression (:postfix-expression) unary-expression-postfix)
    (production :unary-expression (delete :postfix-expression-or-super) unary-expression-delete)
    (production :unary-expression (void :unary-expression) unary-expression-void)
    (production :unary-expression (typeof :unary-expression) unary-expression-typeof)
    (production :unary-expression (++ :postfix-expression-or-super) unary-expression-increment)
    (production :unary-expression (-- :postfix-expression-or-super) unary-expression-decrement)
    (production :unary-expression (+ :unary-expression-or-super) unary-expression-plus)
    (production :unary-expression (- :unary-expression-or-super) unary-expression-minus)
    (production :unary-expression (~ :unary-expression-or-super) unary-expression-bitwise-not)
    (production :unary-expression (! :unary-expression) unary-expression-logical-not)
    
    (production :unary-expression-or-super (:unary-expression) unary-expression-or-super-unary-expression)
    (production :unary-expression-or-super (:super-expression) unary-expression-or-super-super)
    
    
    (%subsection "Multiplicative Operators")
    (rule :multiplicative-expression ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :multiplicative-expression (:multiplicative-expression-or-super * :unary-expression-or-super) multiplicative-expression-multiply
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :multiplicative-expression (:multiplicative-expression-or-super / :unary-expression-or-super) multiplicative-expression-divide
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo)))
      (production :multiplicative-expression (:multiplicative-expression-or-super % :unary-expression-or-super) multiplicative-expression-remainder
        ((verify (s :unused)) (todo))
        ((eval (e :unused)) (todo))))
    (%print-actions)
    
    (rule :multiplicative-expression-or-super ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
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
    (rule :additive-expression ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return ((bin-op-eval bin-op-add sa sb) a b))))
      (production :additive-expression (:additive-expression-or-super - :multiplicative-expression-or-super) additive-expression-subtract
        ((verify s)
         ((verify :additive-expression-or-super) s)
         ((verify :multiplicative-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :additive-expression-or-super) e)))
         (const b object (read-reference ((eval :multiplicative-expression-or-super) e)))
         (const sa class-opt ((super :additive-expression-or-super) e))
         (const sb class-opt ((super :multiplicative-expression-or-super) e))
         (return ((bin-op-eval bin-op-subtract sa sb) a b)))))
    (%print-actions)
    
    (rule :additive-expression-or-super ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
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
    (rule :shift-expression ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return ((bin-op-eval bin-op-shift-left sa sb) a b))))
      (production :shift-expression (:shift-expression-or-super >> :additive-expression-or-super) shift-expression-right-signed
        ((verify s)
         ((verify :shift-expression-or-super) s)
         ((verify :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return ((bin-op-eval bin-op-shift-right sa sb) a b))))
      (production :shift-expression (:shift-expression-or-super >>> :additive-expression-or-super) shift-expression-right-unsigned
        ((verify s)
         ((verify :shift-expression-or-super) s)
         ((verify :additive-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :shift-expression-or-super) e)))
         (const b object (read-reference ((eval :additive-expression-or-super) e)))
         (const sa class-opt ((super :shift-expression-or-super) e))
         (const sb class-opt ((super :additive-expression-or-super) e))
         (return ((bin-op-eval bin-op-shift-right-unsigned sa sb) a b)))))
    (%print-actions)
    
    (rule :shift-expression-or-super ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
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
    (rule (:relational-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return ((bin-op-eval bin-op-less sa sb) a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) > :shift-expression-or-super) relational-expression-greater
        ((verify s)
         ((verify :relational-expression-or-super) s)
         ((verify :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return ((bin-op-eval bin-op-less sb sa) b a))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) <= :shift-expression-or-super) relational-expression-less-or-equal
        ((verify s)
         ((verify :relational-expression-or-super) s)
         ((verify :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return ((bin-op-eval bin-op-less-or-equal sa sb) a b))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) >= :shift-expression-or-super) relational-expression-greater-or-equal
        ((verify s)
         ((verify :relational-expression-or-super) s)
         ((verify :shift-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :relational-expression-or-super) e)))
         (const b object (read-reference ((eval :shift-expression-or-super) e)))
         (const sa class-opt ((super :relational-expression-or-super) e))
         (const sb class-opt ((super :shift-expression-or-super) e))
         (return ((bin-op-eval bin-op-less-or-equal sb sa) b a))))
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
        ((eval (e :unused)) (todo))))
    (%print-actions)
    
    (rule (:relational-expression-or-super :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
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
    (rule (:equality-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return ((bin-op-eval bin-op-equal sa sb) a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) != (:relational-expression-or-super :beta)) equality-expression-not-equal
        ((verify s)
         ((verify :equality-expression-or-super) s)
         ((verify :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not ((bin-op-eval bin-op-equal sa sb) a b)))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) === (:relational-expression-or-super :beta)) equality-expression-strict-equal
        ((verify s)
         ((verify :equality-expression-or-super) s)
         ((verify :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return ((bin-op-eval bin-op-strict-equal sa sb) a b))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) !== (:relational-expression-or-super :beta)) equality-expression-strict-not-equal
        ((verify s)
         ((verify :equality-expression-or-super) s)
         ((verify :relational-expression-or-super) s))
        ((eval e)
         (const a object (read-reference ((eval :equality-expression-or-super) e)))
         (const b object (read-reference ((eval :relational-expression-or-super) e)))
         (const sa class-opt ((super :equality-expression-or-super) e))
         (const sb class-opt ((super :relational-expression-or-super) e))
         (return (unary-not ((bin-op-eval bin-op-strict-equal sa sb) a b))))))
    (%print-actions)
    
    (rule (:equality-expression-or-super :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
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
    (rule (:bitwise-and-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return ((bin-op-eval bin-op-bitwise-and sa sb) a b)))))
    
    (rule (:bitwise-xor-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return ((bin-op-eval bin-op-bitwise-xor sa sb) a b)))))
    
    (rule (:bitwise-or-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return ((bin-op-eval bin-op-bitwise-or sa sb) a b)))))
    (%print-actions)
    
    
    (rule (:bitwise-and-expression-or-super :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-and-expression-or-super :beta) ((:bitwise-and-expression :beta)) bitwise-and-expression-or-super-bitwise-and-expression
        (verify (verify :bitwise-and-expression))
        (eval (eval :bitwise-and-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-and-expression-or-super :beta) (:super-expression) bitwise-and-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-xor-expression-or-super :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-xor-expression-or-super :beta) ((:bitwise-xor-expression :beta)) bitwise-xor-expression-or-super-bitwise-xor-expression
        (verify (verify :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression))
        ((super (e :unused)) (return null)))
      (production (:bitwise-xor-expression-or-super :beta) (:super-expression) bitwise-xor-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-or-expression-or-super :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
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
    (rule (:logical-and-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
    
    (rule (:logical-xor-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
    
    (rule (:logical-or-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
    (rule (:conditional-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
    (rule (:assignment-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
         (return (eval-assignment-op (bin-op-eval (bin-op :compound-assignment) ((super :postfix-expression-or-super) e) null)
                                     (eval :postfix-expression-or-super) (eval :assignment-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment :super-expression) assignment-expression-compound-super
        ((verify s)
         ((verify :postfix-expression-or-super) s)
         ((verify :super-expression) s))
        ((eval e)
         (return (eval-assignment-op (bin-op-eval (bin-op :compound-assignment) ((super :postfix-expression-or-super) e) ((super :super-expression) e))
                                     (eval :postfix-expression-or-super) (eval :super-expression) e))))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((verify s)
         ((verify :postfix-expression) s)
         ((verify :assignment-expression) s))
        ((eval (e :unused)) (todo))))
    
    (define (eval-assignment-op (op (-> (object object) object)) (left-eval (-> (dynamic-env) reference)) (right-eval (-> (dynamic-env) reference)) (e dynamic-env)) reference
      (const r-left reference (left-eval e))
      (const v-left object (read-reference r-left))
      (const v-right object (read-reference (right-eval e)))
      (const result object (op v-left v-right))
      (write-reference r-left result)
      (return result))
    (%print-actions)
    
    (rule :compound-assignment ((bin-op binary-operator))
      (production :compound-assignment (*=) compound-assignment-multiply (bin-op bin-op-multiply))
      (production :compound-assignment (/=) compound-assignment-divide (bin-op bin-op-divide))
      (production :compound-assignment (%=) compound-assignment-remainder (bin-op bin-op-remainder))
      (production :compound-assignment (+=) compound-assignment-add (bin-op bin-op-add))
      (production :compound-assignment (-=) compound-assignment-subtract (bin-op bin-op-subtract))
      (production :compound-assignment (<<=) compound-assignment-shift-left (bin-op bin-op-shift-left))
      (production :compound-assignment (>>=) compound-assignment-shift-right (bin-op bin-op-shift-right))
      (production :compound-assignment (>>>=) compound-assignment-shift-right-unsigned (bin-op bin-op-shift-right-unsigned))
      (production :compound-assignment (&=) compound-assignment-bitwise-and (bin-op bin-op-bitwise-and))
      (production :compound-assignment (^=) compound-assignment-bitwise-xor (bin-op bin-op-bitwise-xor))
      (production :compound-assignment (\|=) compound-assignment-bitwise-or (bin-op bin-op-bitwise-or)))
    (%print-actions)
    
    (production :logical-assignment (&&=) logical-assignment-logical-and)
    (production :logical-assignment (^^=) logical-assignment-logical-xor)
    (production :logical-assignment (\|\|=) logical-assignment-logical-or)
    (%print-actions)
    
    
    (%subsection "Comma Expressions")
    (rule (:list-expression :beta) ((verify (-> (static-env) void)) (eval (-> (dynamic-env) reference)))
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
    
    (rule (:statement :omega) ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
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
    
    (rule (:substatement :omega) ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
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
    (rule :expression-statement ((verify (-> (static-env) void)) (eval (-> (dynamic-env) object)))
      (production :expression-statement ((:- function {) (:list-expression allow-in)) expression-statement-list-expression
        (verify (verify :list-expression))
        ((eval e) (return (read-reference ((eval :list-expression) e))))))
    (%print-actions)
    
    
    (%subsection "Super Statement")
    (production :super-statement (super :arguments) super-statement-super-arguments)
    (%print-actions)
    
    
    (%subsection "Block Statement")
    (rule :annotated-block ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
      (production :annotated-block (:attributes :block) annotated-block-attributes-and-block
        (verify (verify :block)) ;******
        (eval (eval :block)))) ;******
    
    (rule :block ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
      (production :block ({ :directives }) block-directives
        (verify (verify :directives))
        (eval (eval :directives))))
    
    (rule :directives ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
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
    
    (rule :directives-prefix ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
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
    (rule (:labeled-statement :omega) ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((verify s) ((verify :substatement) (add-label s (name :identifier))))
        ((eval e d)
         (catch ((return ((eval :substatement) e d)))
           (x) (if (:narrow-true (in (tag go-break) x))
                 (if (= (& label x) (name :identifier) string)
                   (return (& value x))
                   (throw x))
                 (throw x))))))
    (%print-actions)
    
    
    (%subsection "If Statement")
    (rule (:if-statement :omega) ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
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
    (rule :continue-statement ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (tag go-continue d ""))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (tag go-continue d (name :identifier))))))
    
    (rule :break-statement ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
      (production :break-statement (break) break-statement-unlabeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (tag go-break d ""))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((verify (s :unused)) (todo))
        ((eval (e :unused) d) (throw (tag go-break d (name :identifier))))))
    (%print-actions)
    
    
    (%subsection "Return Statement")
    (rule :return-statement ((verify (-> (static-env) void)) (eval (-> (dynamic-env) object)))
      (production :return-statement (return) return-statement-default
        ((verify s)
         (when (not (& can-return s))
           (throw syntax-error)))
        ((eval (e :unused)) (throw (tag go-return undefined))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((verify s)
         (when (not (& can-return s))
           (throw syntax-error))
         ((verify :list-expression) s))
        ((eval e) (throw (tag go-return (read-reference ((eval :list-expression) e)))))))
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
    (rule (:directive :omega_2) ((verify (-> (static-env) void)) (eval (-> (dynamic-env object) object)))
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
    
    (production :parameter (:parameter-attributes (:typed-identifier allow-in)) parameter-typed-identifier)
    
    (production :parameter-attributes () parameter-attributes-none)
    (production :parameter-attributes (const) parameter-attributes-const)
    
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
          ((verify :directives) initial-static-env)
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
           (depict-list markup-stream #'depict-terminal bin-terminals :separator '(" " :spc " "))))))
    
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
