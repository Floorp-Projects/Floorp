;;;
;;; JavaScript 2.0 parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(defparameter *jw-source* 
  '((line-grammar code-grammar :lr-1 :program)
    
    (%subsection :semantics "Errors")
    (deftype semantic-exception (oneof syntax-error
                                       reference-error
                                       type-error
                                       method-not-found
                                       (break value-and-label)
                                       (continue value-and-label)
                                       (return value)))
    
    (deftype value-and-label (tuple (value value) (label string)))
    
    
    (%subsection :semantics "Values")
    (deftype value (oneof
                    undefined
                    null
                    (boolean boolean)
                    (number float64)
                    (string string)
                    (namespace namespace)
                    (class class)
                    (object object)))
    
    (%text :comment "Return " (:local v) "'s most specific type.")
    (define (value-type (v value)) class
      (case v
        (undefined undefined-class)
        (null null-class)
        (boolean boolean-class)
        (number number-class)
        (string string-class)
        (namespace namespace-class)
        (class class-class)
        ((object o object) (& type o))))
    
    (define (to-boolean (v value)) boolean
      (case v
        (undefined false)
        (null false)
        ((boolean b boolean) b)
        ((number x float64) (not (or (float64-is-zero x) (float64-is-na-n x))))
        ((string s string) (string/= s ""))
        (namespace true)
        (class true)
        (object (todo))))
    
    (define (to-number (v value)) float64
      (case v
        (undefined nan)
        (null 0.0)
        ((boolean b boolean) (if b 1.0 0.0))
        ((number x float64) x)
        ((string s string :unused) (todo))
        (namespace (throw (oneof type-error)))
        (class (throw (oneof type-error)))
        (object (todo))))
    
    (define (to-string (v value)) string
      (case v
        (undefined "undefined")
        (null "null")
        ((boolean b boolean) (if b "true" "false"))
        ((number x float64 :unused) (todo))
        ((string s string) s)
        (namespace (todo))
        (class (todo))
        (object (todo))))
    
    (define (to-primitive (v value) (hint value :unused)) value
      (case v
        (((undefined null boolean number string)) v)
        (((namespace class object)) (oneof string (to-string v)))))
    
    (define (u-int32-to-int32 (i integer)) integer
      (if (< i (^ 2 31))
        i
        (- i (^ 2 32))))
    
    (define (to-u-int32 (x float64)) integer
      (if (or (float64-is-na-n x) (float64-is-infinite x))
        0
        (mod (truncate-float64 x) (^ 2 32))))
    
    (define (to-int32 (x float64)) integer
      (u-int32-to-int32 (to-u-int32 x)))
    
    (define (float64-equal (x float64) (y float64)) boolean
      (float64-compare x y false true false false))
    
    
    (%subsection :semantics "References")
    (deftype reference (oneof (rval value) ref))
    
    (%text :comment "Read the " (:type reference) ".")
    (define (get-value (r reference)) value
      (case r
        ((rval v value) v)
        (ref (todo))))
    
    (%text :comment "Write " (:local v) " into the " (:type reference) ". Return " (:local v) ".")
    (define (put-value (r reference) (v value)) value
      (case r
        ((rval v value) (throw (oneof reference-error)))
        (ref (todo))))
    
    
    (%subsection :semantics "Namespaces")
    (deftype namespace (tuple
                         (id id)))
    (deftype ns (oneof no-ns (ns namespace)))
    
    (define public-namespace namespace (tuple namespace unique))
    
    
    (%subsection :semantics "Classes")
    (deftype class (tuple
                     (id id)
                     (superclass class-opt)
                     (globals (address (vector property)))
                     (prototype (address value))
                     (primitive boolean)))
    (deftype class-opt (oneof no-cls (cls class)))
    
    (define object-class class (tuple class unique (oneof no-cls) (new (vector-of property)) (new (oneof null)) true))
    (define undefined-class class (tuple class unique (oneof cls object-class) (new (vector-of property)) (new (oneof null)) true))
    (define null-class class (tuple class unique (oneof cls object-class) (new (vector-of property)) (new (oneof null)) true))
    (define boolean-class class (tuple class unique (oneof cls object-class) (new (vector-of property)) (new (oneof null)) true))
    (define number-class class (tuple class unique (oneof cls object-class) (new (vector-of property)) (new (oneof null)) true))
    (define string-class class (tuple class unique (oneof cls object-class) (new (vector-of property)) (new (oneof null)) true))
    (define namespace-class class (tuple class unique (oneof cls object-class) (new (vector-of property)) (new (oneof null)) false))
    (define class-class class (tuple class unique (oneof cls object-class) (new (vector-of property)) (new (oneof null)) false))
    
    (define (same-class (c class) (d class)) boolean
      (id= (& id c) (& id d)))
    
    (%text :comment "Return " (:global true) " if " (:local c) " is " (:local d) " or a subclass of " (:local d) ".")
    (define (is-subclass (c class) (d class)) boolean
      (if (id= (& id c) (& id d))
        true
        (case (& superclass c)
          (no-cls false)
          ((cls c-super class) (is-subclass c-super d)))))
    
    (%text :comment "Return " (:global true) " if " (:local c) " is a subclass of " (:local d) " other than " (:local d) " itself.")
    (define (is-proper-subclass (c class) (d class)) boolean
      (case (& superclass c)
        (no-cls false)
        ((cls c-super class) (is-subclass c-super d))))
    
    (%text :comment "Return " (:global true) " if " (:local v) " is an instance of class " (:local c) ". Consider "
           (:character-literal "null") " to be an instance of the classes " (:character-literal "Null") " and "
           (:character-literal "Object") " only.")
    (define (instance-of (v value) (c class)) boolean
      (is-subclass (value-type v) c))
    
    (%text :comment "Return " (:global true) " if " (:local v) " is an instance of class " (:local c) ". Consider "
           (:character-literal "null") " to be an instance of the classes " (:character-literal "Null") ", "
           (:character-literal "Object") ", and all other non-primitive classes.")
    (define (member-of (v value) (c class)) boolean
      (let ((t class (value-type v)))
        (or (is-subclass t c)
            (and (is null v) (not (& primitive c))))))
    
    
    (%subsection :semantics "Objects")
    (deftype object (tuple
                      (id id)
                      (type class)
                      (slots (address (vector slot)))
                      (properties (address (vector property)))))
    
    
    (%subsection :semantics "Slots")
    (deftype slot (tuple
                    (id id)
                    (type class)
                    (v (address value))))
    
    (define (find-slot (id id) (slots (vector slot))) slot
      (if (empty slots)
        (bottom)
        (let ((s slot (nth slots 0)))
          (if (id= id (& id s))
            s
            (find-slot id (subseq slots 1))))))
    
    
    (%subsection :semantics "Properties")
    (deftype property (tuple
                        (name qualified-name)
                        (getter accessor)
                        (setter accessor)
                        (fixed boolean)
                        (enumerable boolean)
                        (deletable boolean)))
    (deftype property-opt (oneof no-prop (prop property)))
    
    (deftype qualified-name (tuple (namespace namespace) (name string)))
    
    (define (same-qualified-name (m qualified-name) (n qualified-name)) boolean
      (and (id= (& id (& namespace m)) (& id (& namespace n)))
           (string= (& name m) (& name n))))
    
    (define (find-property (n qualified-name) (properties (vector property))) (vector property)
      (map properties p p (same-qualified-name n (& name p))))
    
    
    (%subsection :semantics "Accessors")
    (deftype accessor (tuple
                        (self-type class)
                        (final boolean)
                        (data accessor-data)))
    
    (deftype accessor-data (oneof
                            inaccessible
                            abstract
                            (constant value)
                            (slot-id id)
                            (indirect value)
                            (alias qualified-name)))
    
    
    (%subsection :semantics "Binary Operators")
    (deftype binary-operator (tuple
                               (left-type class)
                               (right-type class)
                               (right-final boolean)
                               (left-final boolean)
                               (data bin-op-data)))
    
    (deftype bin-op-data (oneof
                          (built-in (-> (value value) value))
                          (user value)))
    
    (%text :comment "Return " (:global true) " if " (:local op1) " is at least as specific as " (:local op2) ".")
    (define (is-bin-op-subclass (op1 binary-operator) (op2 binary-operator)) boolean
      (and (is-subclass (& left-type op1) (& left-type op2))
           (is-subclass (& right-type op1) (& right-type op2))))
    
    (define (limited-instance-of (v value) (c class) (limit class-opt)) boolean
      (and (instance-of v c)
           (case limit
             (no-cls true)
             ((cls l class) (is-proper-subclass l c)))))
    
    (%text :comment "Return a function that takes two " (:type value) " arguments " (:local left) " and " (:local right)
           " and returns the operator given by " (:local op-table) " applied to " (:local left) " and " (:local right)
           ". If " (:local left-limit) " is a " (:field cls class-opt) " " (:type class)
           ", restrict the lookup to operator definitions with a superclass of " (:local left-limit)
           " for the left operand. Similarly, if " (:local right-limit) " is a " (:field cls class-opt) " " (:type class)
           ", restrict the lookup to operator definitions with a superclass of " (:local right-limit) " for the right operand.")
    (define (bin-op-eval (op-table (vector binary-operator)) (left-limit class-opt) (right-limit class-opt)) (-> (value value) value)
      (function ((left value) (right value))
        (let ((applicable-ops (vector binary-operator)
                              (map op-table op op (and (limited-instance-of left (& left-type op) left-limit)
                                                       (limited-instance-of right (& right-type op) right-limit)))))
          (let ((best-ops (vector binary-operator)
                          (map applicable-ops op op
                               (empty (map applicable-ops op2 op2 (not (is-bin-op-subclass op op2)))))))
            (if (empty best-ops)
              (throw (oneof method-not-found))
              (let ((op binary-operator (nth best-ops 0)))
                (case (& data op)
                  ((built-in f (-> (value value) value)) (f left right))
                  (user (todo)))))))))
    
    
    (%subsection :semantics "Binary Operator Tables")
    
    (define (add-objects (a value) (b value)) value
      (let ((ap value (to-primitive a (oneof null)))
            (bp value (to-primitive b (oneof null))))
        (if (or (is string ap) (is string bp))
          (oneof string (append (to-string ap) (to-string bp)))
          (oneof number (float64-add (to-number ap) (to-number bp))))))
    
    (define (subtract-objects (a value) (b value)) value
      (oneof number (float64-subtract (to-number a) (to-number b))))
    
    (define (multiply-objects (a value) (b value)) value
      (oneof number (float64-multiply (to-number a) (to-number b))))
    
    (define (divide-objects (a value) (b value)) value
      (oneof number (float64-divide (to-number a) (to-number b))))
    
    (define (remainder-objects (a value) (b value)) value
      (oneof number (float64-remainder (to-number a) (to-number b))))
    
    
    (define (less-objects (a value) (b value)) value
      (let ((ap value (to-primitive a (oneof null)))
            (bp value (to-primitive b (oneof null))))
        (oneof boolean
               (if (and (is string ap) (is string bp))
                 (string< (select string ap) (select string bp))
                 (float64-compare (to-number ap) (to-number bp) true false false false)))))
    
    (define (less-or-equal-objects (a value) (b value)) value
      (let ((ap value (to-primitive a (oneof null)))
            (bp value (to-primitive b (oneof null))))
        (oneof boolean
               (if (and (is string ap) (is string bp))
                 (string<= (select string ap) (select string bp))
                 (float64-compare (to-number ap) (to-number bp) true true false false)))))
    
    (define (equal-objects (a value) (b value)) value
      (case a
        (((undefined null)) (oneof boolean (or (is undefined b) (is null b))))
        ((boolean ab boolean) (if (is boolean b)
                                (oneof boolean (not (xor ab (select boolean b))))
                                (equal-objects (oneof number (to-number a)) b)))
        ((number x float64)
         (let ((bp value (to-primitive b (oneof null))))
           (case bp
             (((undefined null namespace class object)) (oneof boolean false))
             (((boolean string number)) (oneof boolean (float64-equal x (to-number bp)))))))
        ((string s string)
         (let ((bp value (to-primitive b (oneof null))))
           (case bp
             (((undefined null namespace class object)) (oneof boolean false))
             (((boolean number)) (oneof boolean (float64-equal (to-number a) (to-number bp))))
             ((string t string) (oneof boolean (string= s t))))))
        (((namespace class object))
         (case b
           (((undefined null)) (oneof boolean false))
           (((namespace class object)) (strict-equal-objects a b))
           (((boolean number string))
            (let ((ap value (to-primitive a (oneof null))))
              (case ap
                (((undefined null namespace class object)) (oneof boolean false))
                (((boolean number string)) (equal-objects ap b)))))))))
    
    (define (strict-equal-objects (a value) (b value)) value
      (oneof boolean
             (case a
               (undefined (is undefined b))
               (null (is null b))
               ((boolean ab boolean) (if (is boolean b) (not (xor ab (select boolean b))) false))
               ((number x float64) (if (is number b) (float64-equal x (select number b)) false))
               ((string s string) (if (is string b) (string= s (select string b)) false))
               ((namespace n namespace) (if (is namespace b) (id= (& id n) (& id (select namespace b))) false))
               ((class c class) (if (is class b) (id= (& id c) (& id (select class b))) false))
               ((object o object) (if (is object b) (id= (& id o) (& id (select object b))) false)))))
    
    
    (define (shift-left-objects (a value) (b value)) value
      (let ((i integer (to-u-int32 (to-number a)))
            (count integer (bitwise-and (to-u-int32 (to-number b)) (hex #x1F))))
        (oneof number (rational-to-float64 (u-int32-to-int32 (bitwise-and (bitwise-shift i count) (hex #xFFFFFFFF)))))))
    
    (define (shift-right-objects (a value) (b value)) value
      (let ((i integer (to-int32 (to-number a)))
            (count integer (bitwise-and (to-u-int32 (to-number b)) (hex #x1F))))
        (oneof number (rational-to-float64 (bitwise-shift i (neg count))))))
    
    (define (shift-right-unsigned-objects (a value) (b value)) value
      (let ((i integer (to-u-int32 (to-number a)))
            (count integer (bitwise-and (to-u-int32 (to-number b)) (hex #x1F))))
        (oneof number (rational-to-float64 (bitwise-shift i (neg count))))))
    
    (define (bitwise-and-objects (a value) (b value)) value
      (let ((i integer (to-int32 (to-number a)))
            (j integer (to-int32 (to-number b))))
        (oneof number (rational-to-float64 (bitwise-and i j)))))
    
    (define (bitwise-xor-objects (a value) (b value)) value
      (let ((i integer (to-int32 (to-number a)))
            (j integer (to-int32 (to-number b))))
        (oneof number (rational-to-float64 (bitwise-xor i j)))))
    
    (define (bitwise-or-objects (a value) (b value)) value
      (let ((i integer (to-int32 (to-number a)))
            (j integer (to-int32 (to-number b))))
        (oneof number (rational-to-float64 (bitwise-or i j)))))
    
    
    (define (default-bin-op-table (f (-> (value value) value))) (address (vector binary-operator))
      (new (vector
            (tuple binary-operator object-class object-class true true (typed-oneof bin-op-data built-in f)))))
    
    (define add-table (address (vector binary-operator)) (default-bin-op-table add-objects))
    (define subtract-table (address (vector binary-operator)) (default-bin-op-table subtract-objects))
    (define multiply-table (address (vector binary-operator)) (default-bin-op-table multiply-objects))
    (define divide-table (address (vector binary-operator)) (default-bin-op-table divide-objects))
    (define remainder-table (address (vector binary-operator)) (default-bin-op-table remainder-objects))
    (define less-table (address (vector binary-operator)) (default-bin-op-table less-objects))
    (define less-or-equal-table (address (vector binary-operator)) (default-bin-op-table less-or-equal-objects))
    (define equal-table (address (vector binary-operator)) (default-bin-op-table equal-objects))
    (define strict-equal-table (address (vector binary-operator)) (default-bin-op-table strict-equal-objects))
    (define shift-left-table (address (vector binary-operator)) (default-bin-op-table shift-left-objects))
    (define shift-right-table (address (vector binary-operator)) (default-bin-op-table shift-right-objects))
    (define shift-right-unsigned-table (address (vector binary-operator)) (default-bin-op-table shift-right-unsigned-objects))
    (define bitwise-and-table (address (vector binary-operator)) (default-bin-op-table bitwise-and-objects))
    (define bitwise-xor-table (address (vector binary-operator)) (default-bin-op-table bitwise-xor-objects))
    (define bitwise-or-table (address (vector binary-operator)) (default-bin-op-table bitwise-or-objects))
    
    
    (%subsection :semantics "Unary Operators")
    (deftype unary-operator (tuple
                              (operand-type class)
                              (final boolean)
                              (data unary-operator-data)))
    
    (deftype unary-operator-data (oneof
                                  (built-in (-> (value) value))
                                  (user value)))
    
    (define (unary-not (a value)) value
      (oneof boolean (not (to-boolean a))))
    
    
    (%subsection :semantics "Environments")
    (deftype static-env (tuple
                          (enclosing-class class-opt)
                          (labels (vector string))
                          (can-return boolean)
                          (constants (vector definition))))
    
    (define initial-static-env static-env (tuple static-env (oneof no-cls) (vector-of string) false (vector-of definition)))
    
    (%text :comment "Return a " (:type static-env) " with label " (:local l) " prepended to " (:local s) ".")
    (define (add-label (s static-env) (l string)) static-env
      (tuple static-env (& enclosing-class s) (append (vector l) (& labels s)) (& can-return s) (& constants s)))
    
    (%text :comment "Return " (:global true) " if " (:character-literal "super") " is permitted here.")
    (define (allows-super (s static-env)) boolean
      (is cls (& enclosing-class s)))
    
    (deftype dynamic-env (tuple
                           (parent dynamic-env-opt)
                           (definitions (vector definition))))
    (deftype dynamic-env-opt (oneof no-frame (frame dynamic-env)))
    
    (%text :comment "If the " (:type dynamic-env) " is from within a class's body, return that class; otherwise, return " (:field no-cls class-opt) ".")
    (define (lexical-class (e dynamic-env)) class-opt
      (todo))
    
    (define initial-dynamic-env dynamic-env (tuple dynamic-env (oneof no-frame) (vector-of definition)))
    
    (deftype definition (tuple
                          (name qualified-name)
                          (getter accessor)
                          (setter accessor)))
    
    
    (%section "Terminal Actions")
    
    (declare-action name $identifier string)
    (declare-action eval $number float64)
    (declare-action eval $string string)
    
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
    
    (rule :parenthesized-list-expression ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production :parenthesized-list-expression (:parenthesized-expression) parenthesized-list-expression-parenthesized-expression
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo)))
      (production :parenthesized-list-expression (\( (:list-expression allow-in) \, (:assignment-expression allow-in) \)) parenthesized-list-expression-list-expression
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo))))
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
    (production :field-name (:parenthesized-expression) field-name-parenthesized-expression)
    
    
    (%subsection "Array Literals")
    (production :array-literal ([ :element-list ]) array-literal-list)
    
    (production :element-list (:literal-element) element-list-one)
    (production :element-list (:element-list \, :literal-element) element-list-more)
    
    (production :literal-element () literal-element-none)
    (production :literal-element ((:assignment-expression allow-in)) literal-element-assignment-expression)
    
    
    (%subsection "Super Operator")
    (rule :super-expression ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :super-expression (super) super-expression-super
        ((verify (s static-env)) (allows-super s))
        ((eval (e dynamic-env :unused)) (todo))
        ((super (e dynamic-env)) (lexical-class e)))
      (production :super-expression (:full-super-expression) super-expression-full-super-expression
        ((verify (s static-env)) (and (allows-super s) (todo)))
        ((eval (e dynamic-env :unused)) (todo))
        ((super (e dynamic-env)) (lexical-class e))))
    
    (production :full-super-expression (super :parenthesized-expression) full-super-expression-super-parenthesized-expression)
    (%print-actions)
    
    
    (%subsection "Postfix Operators")
    (rule :postfix-expression ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production :postfix-expression (:attribute-expression) postfix-expression-attribute-expression
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo)))
      (production :postfix-expression (:full-postfix-expression) postfix-expression-full-postfix-expression
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo)))
      (production :postfix-expression (:short-new-expression) postfix-expression-short-new-expression
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo))))
    (%print-actions)
    
    (rule :postfix-expression-or-super ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :postfix-expression-or-super (:postfix-expression) postfix-expression-or-super-postfix-expression
        (verify (verify :postfix-expression))
        (eval (eval :postfix-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
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
    (? js2
      (production :full-postfix-expression (:postfix-expression @ :qualified-identifier) full-postfix-expression-coerce)
      (production :full-postfix-expression (:postfix-expression @ :parenthesized-expression) full-postfix-expression-indirect-coerce))
    
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
    
    (production :arguments (\( \)) arguments-none)
    (production :arguments (:parenthesized-list-expression) arguments-unnamed)
    (production :arguments (\( :named-argument-list \)) arguments-named)
    
    (production :named-argument-list (:literal-field) named-argument-list-one)
    (production :named-argument-list ((:list-expression allow-in) \, :literal-field) named-argument-list-unnamed)
    (production :named-argument-list (:named-argument-list \, :literal-field) named-argument-list-more)
    
    
    (%subsection "Unary Operators")
    (production :unary-expression (:postfix-expression) unary-expression-postfix)
    (production :unary-expression (const :postfix-expression-or-super) unary-expression-const)
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
    (rule :multiplicative-expression ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production :multiplicative-expression (:unary-expression) multiplicative-expression-unary
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo)))
      (production :multiplicative-expression (:multiplicative-expression-or-super * :unary-expression-or-super) multiplicative-expression-multiply
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo)))
      (production :multiplicative-expression (:multiplicative-expression-or-super / :unary-expression-or-super) multiplicative-expression-divide
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo)))
      (production :multiplicative-expression (:multiplicative-expression-or-super % :unary-expression-or-super) multiplicative-expression-remainder
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused)) (todo))))
    (%print-actions)
    
    (rule :multiplicative-expression-or-super ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :multiplicative-expression-or-super (:multiplicative-expression) multiplicative-expression-or-super-multiplicative-expression
        (verify (verify :multiplicative-expression))
        (eval (eval :multiplicative-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production :multiplicative-expression-or-super (:super-expression) multiplicative-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Additive Operators")
    (rule :additive-expression ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production :additive-expression (:multiplicative-expression) additive-expression-multiplicative
        (verify (verify :multiplicative-expression))
        (eval (eval :multiplicative-expression)))
      (production :additive-expression (:additive-expression-or-super + :multiplicative-expression-or-super) additive-expression-add
        ((verify (s static-env)) (and ((verify :additive-expression-or-super) s) ((verify :multiplicative-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :additive-expression-or-super) e)))
                                      (b value (get-value ((eval :multiplicative-expression-or-super) e)))
                                      (sa class-opt ((super :additive-expression-or-super) e))
                                      (sb class-opt ((super :multiplicative-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ add-table) sa sb) a b)))))
      (production :additive-expression (:additive-expression-or-super - :multiplicative-expression-or-super) additive-expression-subtract
        ((verify (s static-env)) (and ((verify :additive-expression-or-super) s) ((verify :multiplicative-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :additive-expression-or-super) e)))
                                      (b value (get-value ((eval :multiplicative-expression-or-super) e)))
                                      (sa class-opt ((super :additive-expression-or-super) e))
                                      (sb class-opt ((super :multiplicative-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ subtract-table) sa sb) a b))))))
    (%print-actions)
    
    (rule :additive-expression-or-super ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :additive-expression-or-super (:additive-expression) additive-expression-or-super-additive-expression
        (verify (verify :additive-expression))
        (eval (eval :additive-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production :additive-expression-or-super (:super-expression) additive-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Bitwise Shift Operators")
    (rule :shift-expression ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production :shift-expression (:additive-expression) shift-expression-additive
        (verify (verify :additive-expression))
        (eval (eval :additive-expression)))
      (production :shift-expression (:shift-expression-or-super << :additive-expression-or-super) shift-expression-left
        ((verify (s static-env)) (and ((verify :shift-expression-or-super) s) ((verify :additive-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :shift-expression-or-super) e)))
                                      (b value (get-value ((eval :additive-expression-or-super) e)))
                                      (sa class-opt ((super :shift-expression-or-super) e))
                                      (sb class-opt ((super :additive-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ shift-left-table) sa sb) a b)))))
      (production :shift-expression (:shift-expression-or-super >> :additive-expression-or-super) shift-expression-right-signed
        ((verify (s static-env)) (and ((verify :shift-expression-or-super) s) ((verify :additive-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :shift-expression-or-super) e)))
                                      (b value (get-value ((eval :additive-expression-or-super) e)))
                                      (sa class-opt ((super :shift-expression-or-super) e))
                                      (sb class-opt ((super :additive-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ shift-right-table) sa sb) a b)))))
      (production :shift-expression (:shift-expression-or-super >>> :additive-expression-or-super) shift-expression-right-unsigned
        ((verify (s static-env)) (and ((verify :shift-expression-or-super) s) ((verify :additive-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :shift-expression-or-super) e)))
                                      (b value (get-value ((eval :additive-expression-or-super) e)))
                                      (sa class-opt ((super :shift-expression-or-super) e))
                                      (sb class-opt ((super :additive-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ shift-right-unsigned-table) sa sb) a b))))))
    (%print-actions)
    
    (rule :shift-expression-or-super ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production :shift-expression-or-super (:shift-expression) shift-expression-or-super-shift-expression
        (verify (verify :shift-expression))
        (eval (eval :shift-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production :shift-expression-or-super (:super-expression) shift-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Relational Operators")
    (rule (:relational-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:relational-expression :beta) (:shift-expression) relational-expression-shift
        (verify (verify :shift-expression))
        (eval (eval :shift-expression)))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) < :shift-expression-or-super) relational-expression-less
        ((verify (s static-env)) (and ((verify :relational-expression-or-super) s) ((verify :shift-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :relational-expression-or-super) e)))
                                      (b value (get-value ((eval :shift-expression-or-super) e)))
                                      (sa class-opt ((super :relational-expression-or-super) e))
                                      (sb class-opt ((super :shift-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ less-table) sa sb) a b)))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) > :shift-expression-or-super) relational-expression-greater
        ((verify (s static-env)) (and ((verify :relational-expression-or-super) s) ((verify :shift-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :relational-expression-or-super) e)))
                                      (b value (get-value ((eval :shift-expression-or-super) e)))
                                      (sa class-opt ((super :relational-expression-or-super) e))
                                      (sb class-opt ((super :shift-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ less-table) sb sa) b a)))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) <= :shift-expression-or-super) relational-expression-less-or-equal
        ((verify (s static-env)) (and ((verify :relational-expression-or-super) s) ((verify :shift-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :relational-expression-or-super) e)))
                                      (b value (get-value ((eval :shift-expression-or-super) e)))
                                      (sa class-opt ((super :relational-expression-or-super) e))
                                      (sb class-opt ((super :shift-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ less-or-equal-table) sa sb) a b)))))
      (production (:relational-expression :beta) ((:relational-expression-or-super :beta) >= :shift-expression-or-super) relational-expression-greater-or-equal
        ((verify (s static-env)) (and ((verify :relational-expression-or-super) s) ((verify :shift-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :relational-expression-or-super) e)))
                                      (b value (get-value ((eval :shift-expression-or-super) e)))
                                      (sa class-opt ((super :relational-expression-or-super) e))
                                      (sb class-opt ((super :shift-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ less-or-equal-table) sb sa) b a)))))
      (production (:relational-expression :beta) ((:relational-expression :beta) instanceof :shift-expression) relational-expression-instanceof
        ((verify (s static-env)) (and ((verify :relational-expression) s) ((verify :shift-expression) s)))
        ((eval (e dynamic-env :unused)) (todo)))
      (production (:relational-expression allow-in) ((:relational-expression allow-in) in :shift-expression-or-super) relational-expression-in
        ((verify (s static-env)) (and ((verify :relational-expression) s) ((verify :shift-expression-or-super) s)))
        ((eval (e dynamic-env :unused)) (todo))))
    (%print-actions)
    
    (rule (:relational-expression-or-super :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:relational-expression-or-super :beta) ((:relational-expression :beta)) relational-expression-or-super-relational-expression
        (verify (verify :relational-expression))
        (eval (eval :relational-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production (:relational-expression-or-super :beta) (:super-expression) relational-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Equality Operators")
    (rule (:equality-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:equality-expression :beta) ((:relational-expression :beta)) equality-expression-relational
        (verify (verify :relational-expression))
        (eval (eval :relational-expression)))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) == (:relational-expression-or-super :beta)) equality-expression-equal
        ((verify (s static-env)) (and ((verify :equality-expression-or-super) s) ((verify :relational-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :equality-expression-or-super) e)))
                                      (b value (get-value ((eval :relational-expression-or-super) e)))
                                      (sa class-opt ((super :equality-expression-or-super) e))
                                      (sb class-opt ((super :relational-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ equal-table) sa sb) a b)))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) != (:relational-expression-or-super :beta)) equality-expression-not-equal
        ((verify (s static-env)) (and ((verify :equality-expression-or-super) s) ((verify :relational-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :equality-expression-or-super) e)))
                                      (b value (get-value ((eval :relational-expression-or-super) e)))
                                      (sa class-opt ((super :equality-expression-or-super) e))
                                      (sb class-opt ((super :relational-expression-or-super) e)))
                                  (oneof rval (unary-not ((bin-op-eval (@ equal-table) sa sb) a b))))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) === (:relational-expression-or-super :beta)) equality-expression-strict-equal
        ((verify (s static-env)) (and ((verify :equality-expression-or-super) s) ((verify :relational-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :equality-expression-or-super) e)))
                                      (b value (get-value ((eval :relational-expression-or-super) e)))
                                      (sa class-opt ((super :equality-expression-or-super) e))
                                      (sb class-opt ((super :relational-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ strict-equal-table) sa sb) a b)))))
      (production (:equality-expression :beta) ((:equality-expression-or-super :beta) !== (:relational-expression-or-super :beta)) equality-expression-strict-not-equal
        ((verify (s static-env)) (and ((verify :equality-expression-or-super) s) ((verify :relational-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :equality-expression-or-super) e)))
                                      (b value (get-value ((eval :relational-expression-or-super) e)))
                                      (sa class-opt ((super :equality-expression-or-super) e))
                                      (sb class-opt ((super :relational-expression-or-super) e)))
                                  (oneof rval (unary-not ((bin-op-eval (@ strict-equal-table) sa sb) a b)))))))
    (%print-actions)
    
    (rule (:equality-expression-or-super :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:equality-expression-or-super :beta) ((:equality-expression :beta)) equality-expression-or-super-equality-expression
        (verify (verify :equality-expression))
        (eval (eval :equality-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production (:equality-expression-or-super :beta) (:super-expression) equality-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Binary Bitwise Operators")
    (rule (:bitwise-and-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-and-expression :beta) ((:equality-expression :beta)) bitwise-and-expression-equality
        (verify (verify :equality-expression))
        (eval (eval :equality-expression)))
      (production (:bitwise-and-expression :beta) ((:bitwise-and-expression-or-super :beta) & (:equality-expression-or-super :beta)) bitwise-and-expression-and
        ((verify (s static-env)) (and ((verify :bitwise-and-expression-or-super) s) ((verify :equality-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :bitwise-and-expression-or-super) e)))
                                      (b value (get-value ((eval :equality-expression-or-super) e)))
                                      (sa class-opt ((super :bitwise-and-expression-or-super) e))
                                      (sb class-opt ((super :equality-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ bitwise-and-table) sa sb) a b))))))
    
    (rule (:bitwise-xor-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-and-expression :beta)) bitwise-xor-expression-bitwise-and
        (verify (verify :bitwise-and-expression))
        (eval (eval :bitwise-and-expression)))
      (production (:bitwise-xor-expression :beta) ((:bitwise-xor-expression-or-super :beta) ^ (:bitwise-and-expression-or-super :beta)) bitwise-xor-expression-xor
        ((verify (s static-env)) (and ((verify :bitwise-xor-expression-or-super) s) ((verify :bitwise-and-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :bitwise-xor-expression-or-super) e)))
                                      (b value (get-value ((eval :bitwise-and-expression-or-super) e)))
                                      (sa class-opt ((super :bitwise-xor-expression-or-super) e))
                                      (sb class-opt ((super :bitwise-and-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ bitwise-xor-table) sa sb) a b))))))
    
    (rule (:bitwise-or-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:bitwise-or-expression :beta) ((:bitwise-xor-expression :beta)) bitwise-or-expression-bitwise-xor
        (verify (verify :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression)))
      (production (:bitwise-or-expression :beta) ((:bitwise-or-expression-or-super :beta) \| (:bitwise-xor-expression-or-super :beta)) bitwise-or-expression-or
        ((verify (s static-env)) (and ((verify :bitwise-or-expression-or-super) s) ((verify :bitwise-xor-expression-or-super) s)))
        ((eval (e dynamic-env)) (let ((a value (get-value ((eval :bitwise-or-expression-or-super) e)))
                                      (b value (get-value ((eval :bitwise-xor-expression-or-super) e)))
                                      (sa class-opt ((super :bitwise-or-expression-or-super) e))
                                      (sb class-opt ((super :bitwise-xor-expression-or-super) e)))
                                  (oneof rval ((bin-op-eval (@ bitwise-or-table) sa sb) a b))))))
    (%print-actions)
    
    
    (rule (:bitwise-and-expression-or-super :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-and-expression-or-super :beta) ((:bitwise-and-expression :beta)) bitwise-and-expression-or-super-bitwise-and-expression
        (verify (verify :bitwise-and-expression))
        (eval (eval :bitwise-and-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production (:bitwise-and-expression-or-super :beta) (:super-expression) bitwise-and-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-xor-expression-or-super :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-xor-expression-or-super :beta) ((:bitwise-xor-expression :beta)) bitwise-xor-expression-or-super-bitwise-xor-expression
        (verify (verify :bitwise-xor-expression))
        (eval (eval :bitwise-xor-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production (:bitwise-xor-expression-or-super :beta) (:super-expression) bitwise-xor-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    
    (rule (:bitwise-or-expression-or-super :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)) (super (-> (dynamic-env) class-opt)))
      (production (:bitwise-or-expression-or-super :beta) ((:bitwise-or-expression :beta)) bitwise-or-expression-or-super-bitwise-or-expression
        (verify (verify :bitwise-or-expression))
        (eval (eval :bitwise-or-expression))
        ((super (e dynamic-env :unused)) (oneof no-cls)))
      (production (:bitwise-or-expression-or-super :beta) (:super-expression) bitwise-or-expression-or-super-super
        (verify (verify :super-expression))
        (eval (eval :super-expression))
        (super (super :super-expression))))
    (%print-actions)
    
    
    (%subsection "Binary Logical Operators")
    (rule (:logical-and-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:logical-and-expression :beta) ((:bitwise-or-expression :beta)) logical-and-expression-bitwise-or
        (verify (verify :bitwise-or-expression))
        (eval (eval :bitwise-or-expression)))
      (production (:logical-and-expression :beta) ((:logical-and-expression :beta) && (:bitwise-or-expression :beta)) logical-and-expression-and
        ((verify (s static-env)) (and ((verify :logical-and-expression) s) ((verify :bitwise-or-expression) s)))
        ((eval (e dynamic-env)) (let ((v value (get-value ((eval :logical-and-expression) e))))
                                  (oneof rval (if (to-boolean v) (get-value ((eval :bitwise-or-expression) e)) v))))))
    (%print-actions)
    
    (rule (:logical-xor-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:logical-xor-expression :beta) ((:logical-and-expression :beta)) logical-xor-expression-logical-and
        (verify (verify :logical-and-expression))
        (eval (eval :logical-and-expression)))
      (production (:logical-xor-expression :beta) ((:logical-xor-expression :beta) ^^ (:logical-and-expression :beta)) logical-xor-expression-xor
        ((verify (s static-env)) (and ((verify :logical-xor-expression) s) ((verify :logical-and-expression) s)))
        ((eval (e dynamic-env)) (let ((v1 value (get-value ((eval :logical-xor-expression) e)))
                                      (v2 value (get-value ((eval :logical-and-expression) e))))
                                  (oneof rval (oneof boolean (xor (to-boolean v1) (to-boolean v2))))))))
    (%print-actions)
    
    (rule (:logical-or-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:logical-or-expression :beta) ((:logical-xor-expression :beta)) logical-or-expression-logical-xor
        (verify (verify :logical-xor-expression))
        (eval (eval :logical-xor-expression)))
      (production (:logical-or-expression :beta) ((:logical-or-expression :beta) \|\| (:logical-xor-expression :beta)) logical-or-expression-or
        ((verify (s static-env)) (and ((verify :logical-or-expression) s) ((verify :logical-xor-expression) s)))
        ((eval (e dynamic-env)) (let ((v value (get-value ((eval :logical-or-expression) e))))
                                  (oneof rval (if (to-boolean v) v (get-value ((eval :logical-xor-expression) e))))))))
    (%print-actions)
    
    
    (%subsection "Conditional Operator")
    (rule (:conditional-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta)) conditional-expression-logical-or
        (verify (verify :logical-or-expression))
        (eval (eval :logical-or-expression)))
      (production (:conditional-expression :beta) ((:logical-or-expression :beta) ? (:assignment-expression :beta) \: (:assignment-expression :beta)) conditional-expression-conditional
        ((verify (s static-env)) (and (and ((verify :logical-or-expression) s) ((verify :assignment-expression 1) s)) ((verify :assignment-expression 2) s)))
        ((eval (e dynamic-env)) (if (to-boolean (get-value ((eval :logical-or-expression) e))) ((eval :assignment-expression 1) e) ((eval :assignment-expression 2) e)))))
    (%print-actions)
    
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta)) non-assignment-expression-logical-or)
    (production (:non-assignment-expression :beta) ((:logical-or-expression :beta) ? (:non-assignment-expression :beta) \: (:non-assignment-expression :beta)) non-assignment-expression-conditional)
    (%print-actions)
    
    
    (%subsection "Assignment Operators")
    (rule (:assignment-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:assignment-expression :beta) ((:conditional-expression :beta)) assignment-expression-conditional
        (verify (verify :conditional-expression))
        (eval (eval :conditional-expression)))
      (production (:assignment-expression :beta) (:postfix-expression = (:assignment-expression :beta)) assignment-expression-assignment
        ((verify (s static-env)) (and ((verify :postfix-expression) s) ((verify :assignment-expression) s)))
        ((eval (e dynamic-env)) (oneof rval (put-value ((eval :postfix-expression) e) (get-value ((eval :assignment-expression) e))))))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment (:assignment-expression :beta)) assignment-expression-compound
        ((verify (s static-env)) (and ((verify :postfix-expression-or-super) s) ((verify :assignment-expression) s)))
        ((eval (e dynamic-env)) (eval-assignment-op (bin-op-eval (@ (assignment-op-table :compound-assignment)) ((super :postfix-expression-or-super) e) (oneof no-cls))
                                                    (eval :postfix-expression-or-super) (eval :assignment-expression) e)))
      (production (:assignment-expression :beta) (:postfix-expression-or-super :compound-assignment :super-expression) assignment-expression-compound-super
        ((verify (s static-env)) (and ((verify :postfix-expression-or-super) s) ((verify :super-expression) s)))
        ((eval (e dynamic-env)) (eval-assignment-op (bin-op-eval (@ (assignment-op-table :compound-assignment)) ((super :postfix-expression-or-super) e) ((super :super-expression) e))
                                                    (eval :postfix-expression-or-super) (eval :super-expression) e)))
      (production (:assignment-expression :beta) (:postfix-expression :logical-assignment (:assignment-expression :beta)) assignment-expression-logical-compound
        ((verify (s static-env)) (and ((verify :postfix-expression) s) ((verify :assignment-expression) s)))
        ((eval (e dynamic-env :unused)) (todo))))
    
    (define (eval-assignment-op (f (-> (value value) value)) (left-eval (-> (dynamic-env) reference)) (right-eval (-> (dynamic-env) reference)) (e dynamic-env)) reference
      (let ((r-left reference (left-eval e)))
        (let ((v-left value (get-value r-left))
              (v-right value (get-value (right-eval e))))
          (oneof rval (put-value r-left (f v-left v-right))))))
    (%print-actions)
    
    (rule :compound-assignment ((assignment-op-table (address (vector binary-operator))))
      (production :compound-assignment (*=) compound-assignment-multiply (assignment-op-table multiply-table))
      (production :compound-assignment (/=) compound-assignment-divide (assignment-op-table divide-table))
      (production :compound-assignment (%=) compound-assignment-remainder (assignment-op-table remainder-table))
      (production :compound-assignment (+=) compound-assignment-add (assignment-op-table add-table))
      (production :compound-assignment (-=) compound-assignment-subtract (assignment-op-table subtract-table))
      (production :compound-assignment (<<=) compound-assignment-shift-left (assignment-op-table shift-left-table))
      (production :compound-assignment (>>=) compound-assignment-shift-right (assignment-op-table shift-right-table))
      (production :compound-assignment (>>>=) compound-assignment-shift-right-unsigned (assignment-op-table shift-right-unsigned-table))
      (production :compound-assignment (&=) compound-assignment-bitwise-and (assignment-op-table bitwise-and-table))
      (production :compound-assignment (^=) compound-assignment-bitwise-xor (assignment-op-table bitwise-xor-table))
      (production :compound-assignment (\|=) compound-assignment-bitwise-or (assignment-op-table bitwise-or-table)))
    (%print-actions)
    
    (production :logical-assignment (&&=) logical-assignment-logical-and)
    (production :logical-assignment (^^=) logical-assignment-logical-xor)
    (production :logical-assignment (\|\|=) logical-assignment-logical-or)
    (%print-actions)
    
    
    (%subsection "Comma Expressions")
    (rule (:list-expression :beta) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) reference)))
      (production (:list-expression :beta) ((:assignment-expression :beta)) list-expression-assignment
        (verify (verify :assignment-expression))
        (eval (eval :assignment-expression)))
      (production (:list-expression :beta) ((:list-expression :beta) \, (:assignment-expression :beta)) list-expression-comma
        ((verify (s static-env)) (and ((verify :list-expression) s) ((verify :assignment-expression) s)))
        ((eval (e dynamic-env)) (let ((v1 value (get-value ((eval :list-expression) e)) :unused))
                                  ((eval :assignment-expression) e)))))
    
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
    
    (rule (:statement :omega) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production (:statement :omega) (:empty-statement) statement-empty-statement
        ((verify (s static-env :unused)) true)
        ((eval (e dynamic-env :unused) (d value)) d))
      (production (:statement :omega) (:expression-statement (:semicolon :omega)) statement-expression-statement
        (verify (verify :expression-statement))
        ((eval (e dynamic-env) (d value :unused)) ((eval :expression-statement) e)))
      (production (:statement :omega) (:super-statement (:semicolon :omega)) statement-super-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:statement :omega) (:block-statement) statement-block-statement
        (verify (verify :block-statement))
        (eval (eval :block-statement)))
      (production (:statement :omega) ((:labeled-statement :omega)) statement-labeled-statement
        (verify (verify :labeled-statement))
        (eval (eval :labeled-statement)))
      (production (:statement :omega) ((:if-statement :omega)) statement-if-statement
        (verify (verify :if-statement))
        (eval (eval :if-statement)))
      (production (:statement :omega) (:switch-statement) statement-switch-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:statement :omega) (:do-statement (:semicolon :omega)) statement-do-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:statement :omega) ((:while-statement :omega)) statement-while-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:statement :omega) ((:for-statement :omega)) statement-for-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:statement :omega) ((:with-statement :omega)) statement-with-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:statement :omega) (:continue-statement (:semicolon :omega)) statement-continue-statement
        (verify (verify :continue-statement))
        (eval (eval :continue-statement)))
      (production (:statement :omega) (:break-statement (:semicolon :omega)) statement-break-statement
        (verify (verify :break-statement))
        (eval (eval :break-statement)))
      (production (:statement :omega) (:return-statement (:semicolon :omega)) statement-return-statement
        (verify (verify :return-statement))
        ((eval (e dynamic-env) (d value :unused)) ((eval :return-statement) e)))
      (production (:statement :omega) (:throw-statement (:semicolon :omega)) statement-throw-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:statement :omega) (:try-statement) statement-try-statement
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo))))
    (%print-actions)
    
    (production (:semicolon :omega) (\;) semicolon-semicolon)
    (production (:semicolon :omega) ($virtual-semicolon) semicolon-virtual-semicolon)
    (production (:semicolon abbrev) () semicolon-abbrev)
    (production (:semicolon abbrev-no-short-if) () semicolon-abbrev-no-short-if)
    
    (production (:noninsertable-semicolon :omega_2) (\;) noninsertable-semicolon-semicolon)
    (production (:noninsertable-semicolon abbrev) () noninsertable-semicolon-abbrev)
    
    
    (%subsection "Empty Statement")
    (production :empty-statement (\;) empty-statement-semicolon)
    
    
    (%subsection "Expression Statement")
    (rule :expression-statement ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) value)))
      (production :expression-statement ((:- function { const) (:list-expression allow-in)) expression-statement-list-expression
        (verify (verify :list-expression))
        ((eval (e dynamic-env)) (get-value ((eval :list-expression) e)))))
    (%print-actions)
    
    
    (%subsection "Super Statement")
    (production :super-statement (super :arguments) super-statement-super-arguments)
    (%print-actions)
    
    
    (%subsection "Block Statement")
    (rule :block-statement ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production :block-statement (:block) block-statement-block
        (verify (verify :block))
        (eval (eval :block))))
    
    (rule :block ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production :block ({ :directives }) block-directives
        (verify (verify :directives))
        (eval (eval :directives))))
    
    (rule :directives ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production :directives () directives-none
        ((verify (s static-env :unused)) true)
        ((eval (e dynamic-env :unused) (d value)) d))
      (production :directives (:directives-prefix (:directive abbrev)) directives-more
        ((verify (s static-env)) (and ((verify :directives-prefix) s) ((verify :directive) s)))
        ((eval (e dynamic-env) (d value)) ((eval :directives-prefix) e ((eval :directive) e d)))))
    
    (rule :directives-prefix ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production :directives-prefix () directives-prefix-none
        ((verify (s static-env :unused)) true)
        ((eval (e dynamic-env :unused) (d value)) d))
      (production :directives-prefix (:directives-prefix (:directive full)) directives-prefix-more
        ((verify (s static-env)) (and ((verify :directives-prefix) s) ((verify :directive) s)))
        ((eval (e dynamic-env) (d value)) ((eval :directives-prefix) e ((eval :directive) e d)))))
    (%print-actions)
    
    
    (%subsection "Labeled Statements")
    (rule (:labeled-statement :omega) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production (:labeled-statement :omega) (:identifier \: (:substatement :omega)) labeled-statement-label
        ((verify (s static-env)) ((verify :substatement) (add-label s (name :identifier))))
        ((eval (e dynamic-env) (d value))
         (catch ((eval :substatement) e d)
           (x) (if (is break x)
                 (if (string= (& label (select break x)) (name :identifier))
                   (& value (select break x))
                   (throw x))
                 (throw x))))))
    (%print-actions)
    
    
    (%subsection "If Statement")
    (rule (:if-statement :omega) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production (:if-statement abbrev) (if :parenthesized-list-expression (:substatement abbrev)) if-statement-if-then-abbrev
        ((verify (s static-env)) (and ((verify :parenthesized-list-expression) s) ((verify :substatement) s)))
        ((eval (e dynamic-env) (d value)) (if (to-boolean (get-value ((eval :parenthesized-list-expression) e))) ((eval :substatement) e d) d)))
      (production (:if-statement full) (if :parenthesized-list-expression (:substatement full)) if-statement-if-then-full
        ((verify (s static-env)) (and ((verify :parenthesized-list-expression) s) ((verify :substatement) s)))
        ((eval (e dynamic-env) (d value)) (if (to-boolean (get-value ((eval :parenthesized-list-expression) e))) ((eval :substatement) e d) d)))
      (production (:if-statement :omega) (if :parenthesized-list-expression (:substatement abbrev-no-short-if)
                                             else (:substatement :omega)) if-statement-if-then-else
        ((verify (s static-env)) (and (and ((verify :parenthesized-list-expression) s) ((verify :substatement 1) s)) ((verify :substatement 2) s)))
        ((eval (e dynamic-env) (d value))
         (if (to-boolean (get-value ((eval :parenthesized-list-expression) e))) ((eval :substatement 1) e d) ((eval :substatement 2) e d)))))
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
    (production :for-initializer ((:- const) (:list-expression no-in)) for-initializer-expression)
    (production :for-initializer (:attributes :variable-definition-kind (:variable-binding-list no-in)) for-initializer-variable-definition)
    
    (production :for-in-binding ((:- const) :postfix-expression) for-in-binding-expression)
    (production :for-in-binding (:attributes :variable-definition-kind (:variable-binding no-in)) for-in-binding-variable-definition)
    (%print-actions)
    
    
    (%subsection "With Statement")
    (production (:with-statement :omega) (with :parenthesized-list-expression (:substatement :omega)) with-statement-with)
    (%print-actions)
    
    
    (%subsection "Continue and Break Statements")
    (rule :continue-statement ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production :continue-statement (continue) continue-statement-unlabeled
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value)) (throw (oneof continue (tuple value-and-label d "")))))
      (production :continue-statement (continue :no-line-break :identifier) continue-statement-labeled
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value)) (throw (oneof continue (tuple value-and-label d (name :identifier)))))))
    
    (rule :break-statement ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production :break-statement (break) break-statement-unlabeled
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value)) (throw (oneof break (tuple value-and-label d "")))))
      (production :break-statement (break :no-line-break :identifier) break-statement-labeled
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value)) (throw (oneof break (tuple value-and-label d (name :identifier)))))))
    (%print-actions)
    
    
    (%subsection "Return Statement")
    (rule :return-statement ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env) value)))
      (production :return-statement (return) return-statement-default
        ((verify (s static-env)) (& can-return s))
        ((eval (e dynamic-env :unused)) (throw (oneof return (oneof undefined)))))
      (production :return-statement (return :no-line-break (:list-expression allow-in)) return-statement-expression
        ((verify (s static-env)) (and (& can-return s) ((verify :list-expression) s)))
        ((eval (e dynamic-env)) (throw (oneof return (get-value ((eval :list-expression) e)))))))
    (%print-actions)
    
    
    (%subsection "Throw Statement")
    (production :throw-statement (throw :no-line-break (:list-expression allow-in)) throw-statement-throw)
    (%print-actions)
    
    
    (%subsection "Try Statement")
    (production :try-statement (try :block-statement :catch-clauses) try-statement-catch-clauses)
    (production :try-statement (try :block-statement :finally-clause) try-statement-finally-clause)
    (production :try-statement (try :block-statement :catch-clauses :finally-clause) try-statement-catch-clauses-finally-clause)
    
    (production :catch-clauses (:catch-clause) catch-clauses-one)
    (production :catch-clauses (:catch-clauses :catch-clause) catch-clauses-more)
    
    (production :catch-clause (catch \( :parameter \) :block) catch-clause-block)
    
    (production :finally-clause (finally :block-statement) finally-clause-block-statement)
    (%print-actions)
    
    
    (%section "Directives")
    (rule (:directive :omega_2) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production (:directive :omega_2) ((:statement :omega_2)) directive-statement
        (verify (verify :statement))
        (eval (eval :statement)))
      (production (:directive :omega_2) ((:definition :omega_2)) directive-definition
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:directive :omega_2) (:annotated-block) directive-annotated-block
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:directive :omega_2) (:use-directive (:semicolon :omega_2)) directive-use-directive
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (? js2
        (production (:directive :omega_2) (:include-directive (:semicolon :omega_2)) directive-include-directive
          ((verify (s static-env :unused)) (todo))
          ((eval (e dynamic-env :unused) (d value :unused)) (todo))))
      (production (:directive :omega_2) (:language-directive (:noninsertable-semicolon :omega_2)) directive-language-directive
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo))))
    
    (rule (:substatement :omega) ((verify (-> (static-env) boolean)) (eval (-> (dynamic-env value) value)))
      (production (:substatement :omega) ((:statement :omega)) substatement-statement
        (verify (verify :statement))
        (eval (eval :statement)))
      (production (:substatement :omega) (:simple-variable-definition (:semicolon :omega)) substatement-simple-variable-definition
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo)))
      (production (:substatement :omega) (:simple-annotated-block) substatement-simple-annotated-block
        ((verify (s static-env :unused)) (todo))
        ((eval (e dynamic-env :unused) (d value :unused)) (todo))))
    (%print-actions)
    
    
    (%subsection "Annotated Blocks")
    (production :annotated-block (:nonempty-attributes :block) annotated-block-nonempty-attributes-and-block)
    
    (production :nonempty-attributes (:attribute :no-line-break :attributes) nonempty-attributes-more)
    
    (production :simple-annotated-block (:nonempty-attributes { :substatements }) simple-annotated-block-nonempty-attributes-and-simple-block)
    
    (production :substatements () substatements-none)
    (production :substatements (:substatements-prefix (:substatement abbrev)) substatements-more)
    
    (production :substatements-prefix () substatements-prefix-none)
    (production :substatements-prefix (:substatements-prefix (:substatement full)) substatements-prefix-more)
    
    
    (%subsection "Use Directive")
    (production :use-directive (use :no-line-break namespace :nonassignment-expression-list :use-includes-excludes) use-directive-normal)
    
    (production :nonassignment-expression-list ((:non-assignment-expression allow-in)) nonassignment-expression-list-one)
    (production :nonassignment-expression-list (:nonassignment-expression-list \, (:non-assignment-expression allow-in)) nonassignment-expression-list-more)
    
    (production :use-includes-excludes () use-includes-excludes-none)
    (production :use-includes-excludes (exclude :use-name-patterns) use-includes-excludes-exclude-list)
    (production :use-includes-excludes (include :use-name-patterns) use-includes-excludes-include-list)
    
    (production :use-name-patterns (*) use-name-patterns-wildcard)
    (production :use-name-patterns (:use-name-pattern-list) use-name-patterns-use-name-pattern-list)
    
    (production :use-name-pattern-list (:identifier) use-name-pattern-list-one)
    (production :use-name-pattern-list (:use-name-pattern-list \, :identifier) use-name-pattern-list-more)
    
    #|
    (production :use-name-patterns (:use-name-pattern) use-name-patterns-one)
    (production :use-name-patterns (:use-name-patterns \, :use-name-pattern) use-name-patterns-more)
    
    (production :use-name-pattern (:qualified-wildcard-pattern) use-name-pattern-qualified-wildcard-pattern)
    (production :use-name-pattern (:full-postfix-expression \. :qualified-wildcard-pattern) use-name-pattern-dot-qualified-wildcard-pattern)
    (production :use-name-pattern (:attribute-expression \. :qualified-wildcard-pattern) use-name-pattern-dot-qualified-wildcard-pattern2)
    
    (production :qualified-wildcard-pattern (:qualified-identifier) qualified-wildcard-pattern-qualified-identifier)
    (production :qualified-wildcard-pattern (:wildcard-pattern) qualified-wildcard-pattern-wildcard-pattern)
    (production :qualified-wildcard-pattern (:qualifier \:\: :wildcard-pattern) qualified-wildcard-pattern-qualifier)
    (production :qualified-wildcard-pattern (:parenthesized-expression \:\: :wildcard-pattern) qualified-wildcard-pattern-expression-qualifier)
    
    (production :wildcard-pattern (*) wildcard-pattern-all)
    (production :wildcard-pattern ($regular-expression) wildcard-pattern-regular-expression)
    |#
    
    
    (? js2
      (%subsection "Include Directive")
      (production :include-directive (include :no-line-break $string) include-directive-include))
    
    
    (%section "Language Directive")
    (production :language-directive (use :language-alternatives) language-directive-language-alternatives)
    
    (production :language-alternatives (:language-ids) language-alternatives-one)
    (production :language-alternatives (:language-alternatives \| :language-ids) language-alternatives-more)
    
    (production :language-ids () language-ids-none)
    (production :language-ids (:language-id :language-ids) language-ids-more)
    
    (production :language-id (:identifier) language-id-identifier)
    (production :language-id ($number) language-id-number)
    
    
    (%section "Definitions")
    (production (:definition :omega_2) (:attributes (:annotated-definition :omega_2)) definition-attributes-and-annotated-definition)
    (production (:definition :omega_2) (:package-definition) definition-package-definition)
    
    (production (:annotated-definition :omega_2) (:import-definition (:semicolon :omega_2)) annotated-definition-import-definition)
    (production (:annotated-definition :omega_2) (:export-definition (:semicolon :omega_2)) annotated-definition-export-definition)
    (production (:annotated-definition :omega_2) (:variable-definition (:semicolon :omega_2)) annotated-definition-variable-definition)
    (production (:annotated-definition :omega_2) ((:function-definition :omega_2)) annotated-definition-function-definition)
    (production (:annotated-definition :omega_2) ((:class-definition :omega_2)) annotated-definition-class-definition)
    (production (:annotated-definition :omega_2) (:namespace-definition (:semicolon :omega_2)) annotated-definition-namespace-definition)
    (? js2
      (production (:annotated-definition :omega_2) ((:interface-definition :omega_2)) annotated-definition-interface-definition))
    
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
    
    
    (%subsection "Import Definition")
    (production :import-definition (import :import-binding :import-uses :use-includes-excludes) import-definition-import)
    
    (production :import-binding (:import-item) import-binding-import-item)
    (production :import-binding (:identifier = :import-item) import-binding-named-import-item)
    
    (production :import-item ($string) import-item-string)
    (production :import-item (:package-name) import-item-package-name)
    
    (production :import-uses () import-uses-none)
    (production :import-uses (\: :nonassignment-expression-list) import-uses-nonassignment-expression-list)
    
    
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
    
    (production (:variable-binding :beta) ((:typed-variable :beta)) variable-binding-simple)
    (production (:variable-binding :beta) ((:typed-variable :beta) = (:variable-initializer :beta)) variable-binding-initialized)
    
    (production (:variable-initializer :beta) ((:assignment-expression :beta)) variable-initializer-assignment-expression)
    (production (:variable-initializer :beta) (:multiple-attributes) variable-initializer-multiple-attributes)
    (production (:variable-initializer :beta) (abstract) variable-initializer-abstract)
    (production (:variable-initializer :beta) (final) variable-initializer-final)
    (production (:variable-initializer :beta) (private) variable-initializer-private)
    (production (:variable-initializer :beta) (static) variable-initializer-static)
    
    (production :multiple-attributes (:attribute :no-line-break :attribute) multiple-attributes-two)
    (production :multiple-attributes (:multiple-attributes :no-line-break :attribute) multiple-attributes-more)
    
    (production (:typed-variable :beta) (:identifier) typed-variable-identifier)
    (production (:typed-variable :beta) (:identifier \: (:type-expression :beta)) typed-variable-identifier-and-type)
    ;(production (:typed-variable :beta) ((:type-expression :beta) :identifier) typed-variable-type-and-identifier)
    
    
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
    (production :all-parameters (:optional-named-rest-parameters) all-parameters-optional-named-rest-parameters)
    
    (production :optional-named-rest-parameters (:optional-parameter) optional-named-rest-parameters-optional-parameter)
    (production :optional-named-rest-parameters (:optional-parameter \, :optional-named-rest-parameters) optional-named-rest-parameters-optional-parameter-and-more)
    (production :optional-named-rest-parameters (\| :named-rest-parameters) optional-named-rest-parameters-named-rest-parameters)
    (production :optional-named-rest-parameters (:rest-parameter) optional-named-rest-parameters-rest-parameter)
    (production :optional-named-rest-parameters (:rest-parameter \, \| :named-parameters) optional-named-rest-parameters-rest-and-named)
    
    (production :named-rest-parameters (:named-parameter) named-rest-parameters-named-parameter)
    (production :named-rest-parameters (:named-parameter \, :named-rest-parameters) named-rest-parameters-named-parameter-and-more)
    (production :named-rest-parameters (:rest-parameter) named-rest-parameters-rest-parameter)
    
    (production :named-parameters (:named-parameter) named-parameters-named-parameter)
    (production :named-parameters (:named-parameter \, :named-parameters) named-parameters-named-parameter-and-more)
    
    (production :rest-parameter (\.\.\.) rest-parameter-none)
    (production :rest-parameter (\.\.\. :parameter) rest-parameter-parameter)
    
    (production :parameter (:parameter-attributes :identifier) parameter-identifier)
    (production :parameter (:parameter-attributes :identifier \: (:type-expression allow-in)) parameter-identifier-and-type)
    ;(production :parameter (:parameter-attributes (:- $string const) (:type-expression allow-in) :identifier) parameter-type-and-identifier)
    
    (production :parameter-attributes () parameter-attributes-none)
    (production :parameter-attributes (const) parameter-attributes-const)
    
    (production :optional-parameter (:parameter = (:assignment-expression allow-in)) optional-parameter-assignment-expression)
    
    (production :named-parameter (:parameter) named-parameter-parameter)
    (production :named-parameter (:optional-parameter) named-parameter-optional-parameter)
    (production :named-parameter ($string :named-parameter) named-parameter-name)
    
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
    
    
    (%section "Package Definition")
    (production :package-definition (package :block) package-definition-anonymous)
    (production :package-definition (package :package-name :block) package-definition-named)
    
    (production :package-name (:identifier) package-name-one)
    (production :package-name (:package-name \. :identifier) package-name-more)
    
    
    (%section "Programs")
    (rule :program ((eval-program value))
      (production :program (:directives) program-directives
        (eval-program (if ((verify :directives) initial-static-env)
                        ((eval :directives) initial-dynamic-env (oneof undefined))
                        (throw (oneof syntax-error))))))))


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
                ((member terminal (rule-productions (grammar-rule grammar ':identifier)) :key #'production-first-terminal) 5)
                (t 3)))
             1))
         1))
     
     (depict-terminal-bin (bin-name bin-terminals)
       (when bin-terminals
         (depict-paragraph (markup-stream ':body-text)
           (depict markup-stream bin-name)
           (depict-list markup-stream #'depict-terminal bin-terminals :separator '(" " :spc " "))))))
    
    (let* ((bins (make-array 6 :initial-element nil))
           (all-terminals (grammar-terminals grammar))
           (terminals (remove-if #'lf-terminal? all-terminals)))
      (assert-true (= (length all-terminals) (1- (* 2 (length terminals)))))
      (setf (svref bins 2) (list '\# '&&= '-> '@ '^^ '^^= '\|\|=))
      (setf (svref bins 4) (list 'abstract 'class 'const 'debugger 'enum 'export 'extends 'final 'goto 'implements 'import
                                 'interface 'native 'package 'private 'protected 'public 'static 'super 'synchronized
                                 'throws 'transient 'volatile))
      ; Used to be reserved in JavaScript 1.5: 'boolean 'byte 'char 'double 'float 'int 'long 'short
      (do ((i (length terminals)))
          ((zerop i))
        (let ((terminal (aref terminals (decf i))))
          (unless (eq terminal *end-marker*)
            (setf (svref bins 2) (delete terminal (svref bins 2)))
            (setf (svref bins 4) (delete terminal (svref bins 4)))
            (push terminal (svref bins (terminal-bin terminal))))))
      (depict-paragraph (markup-stream ':section-heading)
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
