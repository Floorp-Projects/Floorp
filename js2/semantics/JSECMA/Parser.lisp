;;; The contents of this file are subject to the Mozilla Public
;;; License Version 1.1 (the "License"); you may not use this file
;;; except in compliance with the License. You may obtain a copy of
;;; the License at http://www.mozilla.org/MPL/
;;; 
;;; Software distributed under the License is distributed on an "AS
;;; IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
;;; implied. See the License for the specific language governing
;;; rights and limitations under the License.
;;; 
;;; The Original Code is the Language Design and Prototyping Environment.
;;; 
;;; The Initial Developer of the Original Code is Netscape Communications
;;; Corporation.  Portions created by Netscape Communications Corporation are
;;; Copyright (C) 1999 Netscape Communications Corporation.  All
;;; Rights Reserved.
;;; 
;;; Contributor(s):   Waldemar Horwat <waldemar@acm.org>

;;;
;;; ECMAScript sample grammar portions
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(declaim (optimize (debug 3)))

(progn
  (defparameter *gw*
    (generate-world
     "G"
     '((grammar code-grammar :lr-1 :program)
       
       (%heading 1 "Types")
       
       (deftype value (oneof undefined-value
                             null-value
                             (boolean-value boolean)
                             (number-value float64)
                             (string-value string)
                             (object-value object)))
       (deftype object-or-null (oneof null-object-or-null (object-object-or-null object)))
       (deftype object (tuple (properties (address (vector property)))
                         (typeof-name string)
                         (prototype object-or-null)
                         (get (-> (prop-name) value-or-exception))
                         (put (-> (prop-name value) void-or-exception))
                         (delete (-> (prop-name) boolean-or-exception))
                         (call (-> (object-or-null (vector value)) reference-or-exception))
                         (construct (-> ((vector value)) object-or-exception))
                         (default-value (-> (default-value-hint) value-or-exception))))
       (deftype default-value-hint (oneof no-hint number-hint string-hint))
       (deftype property (tuple (name string) (read-only boolean) (enumerable boolean) (permanent boolean) (value (address value))))
       
       (deftype prop-name string)
       (deftype place (tuple (base object) (property prop-name)))
       (deftype reference (oneof (value-reference value) (place-reference place) (virtual-reference prop-name)))
       
       
       (deftype integer-or-exception (oneof (normal integer) (abrupt exception)))
       (deftype void-or-exception (oneof normal (abrupt exception)))
       (deftype boolean-or-exception (oneof (normal boolean) (abrupt exception)))
       (deftype float64-or-exception (oneof (normal float64) (abrupt exception)))
       (deftype string-or-exception (oneof (normal string) (abrupt exception)))
       (deftype object-or-exception (oneof (normal object) (abrupt exception)))
       (deftype value-or-exception (oneof (normal value) (abrupt exception)))
       (deftype reference-or-exception (oneof (normal reference) (abrupt exception)))
       (deftype value-list-or-exception (oneof (normal (vector value)) (abrupt exception)))
       
       (%heading 1 "Helper Functions")
       
       (define (object-or-null-to-value (o object-or-null)) value
         (case o
           (null-object-or-null (oneof null-value))
           ((object-object-or-null obj object) (oneof object-value obj))))
       
       (define undefined-result value-or-exception
         (oneof normal (oneof undefined-value)))
       (define null-result value-or-exception
         (oneof normal (oneof null-value)))
       (define (boolean-result (b boolean)) value-or-exception
         (oneof normal (oneof boolean-value b)))
       (define (float64-result (d float64)) value-or-exception
         (oneof normal (oneof number-value d)))
       (define (integer-result (i integer)) value-or-exception
         (float64-result (rational-to-float64 i)))
       (define (string-result (s string)) value-or-exception
         (oneof normal (oneof string-value s)))
       (define (object-result (o object)) value-or-exception
         (oneof normal (oneof object-value o)))
       
       (%heading 1 "Exceptions")
       
       (deftype exception (oneof (exception value) (error error)))
       (deftype error (oneof coerce-to-primitive-error
                             coerce-to-object-error
                             get-value-error
                             put-value-error
                             delete-error))
       
       (define (make-error (err error)) exception
         (oneof error err))
       
       (%heading 1 "Objects")
       
       
       (%heading 1 "Conversions")
       
       (define (reference-get-value (rv reference)) value-or-exception
         (case rv
           ((value-reference v value) (oneof normal v))
           ((place-reference r place) ((& get (& base r)) (& property r)))
           (virtual-reference (typed-oneof value-or-exception abrupt (make-error (oneof get-value-error))))))
       
       (define (reference-put-value (rv reference) (v value)) void-or-exception
         (case rv
           (value-reference (typed-oneof void-or-exception abrupt (make-error (oneof put-value-error))))
           ((place-reference r place) ((& put (& base r)) (& property r) v))
           (virtual-reference (bottom))))
       
       (%heading 1 "Coercions")
       
       (define (coerce-to-boolean (v value)) boolean
         (case v
           (((undefined-value null-value)) false)
           ((boolean-value b boolean) b)
           ((number-value d float64) (not (or (float64-is-zero d) (float64-is-na-n d))))
           ((string-value s string) (/= (length s) 0))
           (object-value true)))
       
       (define (coerce-boolean-to-float64 (b boolean)) float64
         (if b 1.0 0.0))
       
       (define (coerce-to-float64 (v value)) float64-or-exception
         (case v
           (undefined-value (oneof normal nan))
           (null-value (oneof normal 0.0))
           ((boolean-value b boolean) (oneof normal (coerce-boolean-to-float64 b)))
           ((number-value d float64) (oneof normal d))
           (string-value (bottom))
           (object-value (bottom))))
       
       (define (float64-to-uint32 (x float64)) integer
         (if (or (float64-is-na-n x) (float64-is-infinite x))
           0
           (mod (truncate-finite-float64 x) #x100000000)))
       
       (define (coerce-to-uint32 (v value)) integer-or-exception
         (letexc (d float64 (coerce-to-float64 v))
           (oneof normal (float64-to-uint32 d))))
       
       (define (coerce-to-int32 (v value)) integer-or-exception
         (letexc (d float64 (coerce-to-float64 v))
           (oneof normal (uint32-to-int32 (float64-to-uint32 d)))))
       
       (define (uint32-to-int32 (ui integer)) integer
         (if (< ui #x80000000)
           ui
           (- ui #x100000000)))
       
       (define (coerce-to-string (v value)) string-or-exception
         (case v
           (undefined-value (oneof normal "undefined"))
           (null-value (oneof normal "null"))
           ((boolean-value b boolean) (if b (oneof normal "true") (oneof normal "false")))
           (number-value (bottom))
           ((string-value s string) (oneof normal s))
           (object-value (bottom))))
       
       (define (coerce-to-primitive (v value) (hint default-value-hint)) value-or-exception
         (case v
           (((undefined-value null-value boolean-value number-value string-value)) (oneof normal v))
           ((object-value o object)
            (letexc (pv value ((& default-value o) hint))
              (case pv
                (((undefined-value null-value boolean-value number-value string-value)) (oneof normal pv))
                (object-value (typed-oneof value-or-exception abrupt (make-error (oneof coerce-to-primitive-error)))))))))
       
       (define (coerce-to-object (v value)) object-or-exception
         (case v
           (((undefined-value null-value)) (typed-oneof object-or-exception abrupt (make-error (oneof coerce-to-object-error))))
           (boolean-value (bottom))
           (number-value (bottom))
           (string-value (bottom))
           ((object-value o object) (oneof normal o))))
       
       (%heading 1 "Environments")
       
       (deftype env (tuple (this object-or-null)))
       (define (lookup-identifier (e env :unused) (id string :unused)) reference-or-exception
         (bottom))
       
       (%heading 1 "Terminal Actions")
       
       (declare-action eval-identifier $identifier string)
       (declare-action eval-number $number float64)
       (declare-action eval-string $string string)
       
       (terminal-action eval-identifier $identifier cdr)
       (terminal-action eval-number $number cdr)
       (terminal-action eval-string $string cdr)
       (%print-actions)
       
       (%heading 1 "Primary Expressions")
       
       (declare-action eval :primary-rvalue (-> (env) value-or-exception))
       (production :primary-rvalue (this) primary-rvalue-this
         ((eval (e env))
          (oneof normal (object-or-null-to-value (& this e)))))
       (production :primary-rvalue (null) primary-rvalue-null
         ((eval (e env :unused))
          null-result))
       (production :primary-rvalue (true) primary-rvalue-true
         ((eval (e env :unused))
          (boolean-result true)))
       (production :primary-rvalue (false) primary-rvalue-false
         ((eval (e env :unused))
          (boolean-result false)))
       (production :primary-rvalue ($number) primary-rvalue-number
         ((eval (e env :unused))
          (float64-result (eval-number $number))))
       (production :primary-rvalue ($string) primary-rvalue-string
         ((eval (e env :unused))
          (string-result (eval-string $string))))
       (production :primary-rvalue (\( (:comma-expression no-l-value) \)) primary-rvalue-parentheses
         (eval (eval :comma-expression)))
       
       (declare-action eval :primary-lvalue (-> (env) reference-or-exception))
       (production :primary-lvalue ($identifier) primary-lvalue-identifier
         ((eval (e env))
          (lookup-identifier e (eval-identifier $identifier))))
       (production :primary-lvalue (\( :lvalue \)) primary-lvalue-parentheses
         (eval (eval :lvalue)))
       (%print-actions)
       
       (%heading 1 "Left-Side Expressions")
       
       (grammar-argument :expr-kind any-value no-l-value)
       (grammar-argument :member-expr-kind call no-call)
       
       (declare-action eval (:member-lvalue :member-expr-kind) (-> (env) reference-or-exception))
       (production (:member-lvalue no-call) (:primary-lvalue) member-lvalue-primary-lvalue
         (eval (eval :primary-lvalue)))
       (production (:member-lvalue call) (:lvalue :arguments) member-lvalue-call-member-lvalue
         ((eval (e env))
          (letexc (f-reference reference ((eval :lvalue) e))
            (letexc (f value (reference-get-value f-reference))
              (letexc (arguments (vector value) ((eval :arguments) e))
                (let ((this object-or-null
                            (case f-reference
                              (((value-reference virtual-reference)) (oneof null-object-or-null))
                              ((place-reference p place) (oneof object-object-or-null (& base p))))))
                  (call-object f this arguments)))))))
       (production (:member-lvalue call) ((:member-expression no-call no-l-value) :arguments) member-lvalue-call-member-expression-no-call
         ((eval (e env))
          (letexc (f value ((eval :member-expression) e))
            (letexc (arguments (vector value) ((eval :arguments) e))
              (call-object f (oneof null-object-or-null) arguments)))))
       (production (:member-lvalue :member-expr-kind) ((:member-expression :member-expr-kind any-value) \[ :expression \]) member-lvalue-array
         ((eval (e env))
          (letexc (container value ((eval :member-expression) e))
            (letexc (property value ((eval :expression) e))
              (read-property container property)))))
       (production (:member-lvalue :member-expr-kind) ((:member-expression :member-expr-kind any-value) \. $identifier) member-lvalue-property
         ((eval (e env))
          (letexc (container value ((eval :member-expression) e))
            (read-property container (oneof string-value (eval-identifier $identifier))))))
       
       (declare-action eval (:member-expression :member-expr-kind :expr-kind) (-> (env) value-or-exception))
       (%rule (:member-expression no-call no-l-value))
       (%rule (:member-expression no-call any-value))
       (%rule (:member-expression call any-value))
       (production (:member-expression no-call :expr-kind) (:primary-rvalue) member-expression-primary-rvalue
         (eval (eval :primary-rvalue)))
       (production (:member-expression :member-expr-kind any-value) ((:member-lvalue :member-expr-kind)) member-expression-member-lvalue
         ((eval (e env))
          (letexc (ref reference ((eval :member-lvalue) e))
            (reference-get-value ref))))
       (production (:member-expression no-call :expr-kind) (new (:member-expression no-call any-value) :arguments) member-expression-new
         ((eval (e env))
          (letexc (constructor value ((eval :member-expression) e))
            (letexc (arguments (vector value) ((eval :arguments) e))
              (construct-object constructor arguments)))))
       
       (declare-action eval (:new-expression :expr-kind) (-> (env) value-or-exception))
       (production (:new-expression :expr-kind) ((:member-expression no-call :expr-kind)) new-expression-member-expression
         (eval (eval :member-expression)))
       (production (:new-expression :expr-kind) (new (:new-expression any-value)) new-expression-new
         ((eval (e env))
          (letexc (constructor value ((eval :new-expression) e))
            (construct-object constructor (vector-of value)))))
       
       (declare-action eval :arguments (-> (env) value-list-or-exception))
       (production :arguments (\( \)) arguments-empty
         ((eval (e env :unused))
          (oneof normal (vector-of value))))
       (production :arguments (\( :argument-list \)) arguments-list
         (eval (eval :argument-list)))
       
       (declare-action eval :argument-list (-> (env) value-list-or-exception))
       (production :argument-list ((:assignment-expression any-value)) argument-list-one
         ((eval (e env))
          (letexc (arg value ((eval :assignment-expression) e))
            (oneof normal (vector arg)))))
       (production :argument-list (:argument-list \, (:assignment-expression any-value)) argument-list-more
         ((eval (e env))
          (letexc (args (vector value) ((eval :argument-list) e))
            (letexc (arg value ((eval :assignment-expression) e))
              (oneof normal (append args (vector arg)))))))
       
       (declare-action eval :lvalue (-> (env) reference-or-exception))
       (production :lvalue ((:member-lvalue call)) lvalue-member-lvalue-call
         (eval (eval :member-lvalue)))
       (production :lvalue ((:member-lvalue no-call)) lvalue-member-lvalue-no-call
         (eval (eval :member-lvalue)))
       (%print-actions)
       
       (define (read-property (container value) (property value)) reference-or-exception
         (letexc (obj object (coerce-to-object container))
           (letexc (name prop-name (coerce-to-string property))
             (oneof normal (oneof place-reference (tuple place obj name))))))
       
       (define (call-object (f value) (this object-or-null) (arguments (vector value))) reference-or-exception
         (case f
           (((undefined-value null-value boolean-value number-value string-value))
            (typed-oneof reference-or-exception abrupt (make-error (oneof coerce-to-object-error))))
           ((object-value o object)
            ((& call o) this arguments))))
       
       (define (construct-object (constructor value) (arguments (vector value))) value-or-exception
         (case constructor
           (((undefined-value null-value boolean-value number-value string-value))
            (typed-oneof value-or-exception abrupt (make-error (oneof coerce-to-object-error))))
           ((object-value o object)
            (letexc (res object ((& construct o) arguments))
              (object-result res)))))
       
       (%heading 1 "Postfix Expressions")
       
       (declare-action eval (:postfix-expression :expr-kind) (-> (env) value-or-exception))
       (production (:postfix-expression :expr-kind) ((:new-expression :expr-kind)) postfix-expression-new
         (eval (eval :new-expression)))
       (production (:postfix-expression any-value) ((:member-expression call any-value)) postfix-expression-member-expression-call
         (eval (eval :member-expression)))
       (production (:postfix-expression :expr-kind) (:lvalue ++) postfix-expression-increment
         ((eval (e env))
          (letexc (operand-reference reference ((eval :lvalue) e))
            (letexc (operand-value value (reference-get-value operand-reference))
              (letexc (operand float64 (coerce-to-float64 operand-value))
                (letexc (u void (reference-put-value operand-reference (oneof number-value (float64-add operand 1.0)))
                           :unused)
                  (float64-result operand)))))))
       (production (:postfix-expression :expr-kind) (:lvalue --) postfix-expression-decrement
         ((eval (e env))
          (letexc (operand-reference reference ((eval :lvalue) e))
            (letexc (operand-value value (reference-get-value operand-reference))
              (letexc (operand float64 (coerce-to-float64 operand-value))
                (letexc (u void (reference-put-value operand-reference (oneof number-value (float64-subtract operand 1.0)))
                           :unused)
                  (float64-result operand)))))))
       (%print-actions)
       
       (%heading 1 "Unary Operators")
       
       (declare-action eval (:unary-expression :expr-kind) (-> (env) value-or-exception))
       (production (:unary-expression :expr-kind) ((:postfix-expression :expr-kind)) unary-expression-postfix
         (eval (eval :postfix-expression)))
       (production (:unary-expression :expr-kind) (delete :lvalue) unary-expression-delete
         ((eval (e env))
          (letexc (rv reference ((eval :lvalue) e))
            (case rv
              (value-reference (typed-oneof value-or-exception abrupt (make-error (oneof delete-error))))
              ((place-reference r place)
               (letexc (b boolean ((& delete (& base r)) (& property r)))
                 (boolean-result b)))
              (virtual-reference (boolean-result true))))))
       (production (:unary-expression :expr-kind) (void (:unary-expression any-value)) unary-expression-void
         ((eval (e env))
          (letexc (operand value ((eval :unary-expression) e) :unused)
            undefined-result)))
       (production (:unary-expression :expr-kind) (typeof :lvalue) unary-expression-typeof-lvalue
         ((eval (e env))
          (letexc (rv reference ((eval :lvalue) e))
            (case rv
              ((value-reference v value) (string-result (value-typeof v)))
              ((place-reference r place)
               (letexc (v value ((& get (& base r)) (& property r)))
                 (string-result (value-typeof v))))
              (virtual-reference (string-result "undefined"))))))
       (production (:unary-expression :expr-kind) (typeof (:unary-expression no-l-value)) unary-expression-typeof-expression
         ((eval (e env))
          (letexc (v value ((eval :unary-expression) e))
            (string-result (value-typeof v)))))
       (production (:unary-expression :expr-kind) (++ :lvalue) unary-expression-increment
         ((eval (e env))
          (letexc (operand-reference reference ((eval :lvalue) e))
            (letexc (operand-value value (reference-get-value operand-reference))
              (letexc (operand float64 (coerce-to-float64 operand-value))
                (let ((res float64 (float64-add operand 1.0)))
                  (letexc (u void (reference-put-value operand-reference (oneof number-value res)) :unused)
                    (float64-result res))))))))
       (production (:unary-expression :expr-kind) (-- :lvalue) unary-expression-decrement
         ((eval (e env))
          (letexc (operand-reference reference ((eval :lvalue) e))
            (letexc (operand-value value (reference-get-value operand-reference))
              (letexc (operand float64 (coerce-to-float64 operand-value))
                (let ((res float64 (float64-subtract operand 1.0)))
                  (letexc (u void (reference-put-value operand-reference (oneof number-value res)) :unused)
                    (float64-result res))))))))
       (production (:unary-expression :expr-kind) (+ (:unary-expression any-value)) unary-expression-plus
         ((eval (e env))
          (letexc (operand-value value ((eval :unary-expression) e))
            (letexc (operand float64 (coerce-to-float64 operand-value))
              (float64-result operand)))))
       (production (:unary-expression :expr-kind) (- (:unary-expression any-value)) unary-expression-minus
         ((eval (e env))
          (letexc (operand-value value ((eval :unary-expression) e))
            (letexc (operand float64 (coerce-to-float64 operand-value))
              (float64-result (float64-negate operand))))))
       (production (:unary-expression :expr-kind) (~ (:unary-expression any-value)) unary-expression-bitwise-not
         ((eval (e env))
          (letexc (operand-value value ((eval :unary-expression) e))
            (letexc (operand integer (coerce-to-int32 operand-value))
              (integer-result (bitwise-xor operand -1))))))
       (production (:unary-expression :expr-kind) (! (:unary-expression any-value)) unary-expression-logical-not
         ((eval (e env))
          (letexc (operand-value value ((eval :unary-expression) e))
            (boolean-result (not (coerce-to-boolean operand-value))))))
       (%print-actions)
       
       (define (value-typeof (v value)) string
         (case v
           (undefined-value "undefined")
           (null-value "object")
           (boolean-value "boolean")
           (number-value "number")
           (string-value "string")
           ((object-value o object) (& typeof-name o))))
       
       (%heading 1 "Multiplicative Operators")
       
       (declare-action eval (:multiplicative-expression :expr-kind) (-> (env) value-or-exception))
       (production (:multiplicative-expression :expr-kind) ((:unary-expression :expr-kind)) multiplicative-expression-unary
         (eval (eval :unary-expression)))
       (production (:multiplicative-expression :expr-kind) ((:multiplicative-expression any-value) * (:unary-expression any-value)) multiplicative-expression-multiply
         ((eval (e env))
          (letexc (left-value value ((eval :multiplicative-expression) e))
            (letexc (right-value value ((eval :unary-expression) e))
              (apply-binary-float64-operator float64-multiply left-value right-value)))))
       (production (:multiplicative-expression :expr-kind) ((:multiplicative-expression any-value) / (:unary-expression any-value)) multiplicative-expression-divide
         ((eval (e env))
          (letexc (left-value value ((eval :multiplicative-expression) e))
            (letexc (right-value value ((eval :unary-expression) e))
              (apply-binary-float64-operator float64-divide left-value right-value)))))
       (production (:multiplicative-expression :expr-kind) ((:multiplicative-expression any-value) % (:unary-expression any-value)) multiplicative-expression-remainder
         ((eval (e env))
          (letexc (left-value value ((eval :multiplicative-expression) e))
            (letexc (right-value value ((eval :unary-expression) e))
              (apply-binary-float64-operator float64-remainder left-value right-value)))))
       (%print-actions)
       
       (define (apply-binary-float64-operator (operator (-> (float64 float64) float64)) (left-value value) (right-value value)) value-or-exception
         (letexc (left-number float64 (coerce-to-float64 left-value))
           (letexc (right-number float64 (coerce-to-float64 right-value))
             (float64-result (operator left-number right-number)))))
       
       (%heading 1 "Additive Operators")
       
       (declare-action eval (:additive-expression :expr-kind) (-> (env) value-or-exception))
       (production (:additive-expression :expr-kind) ((:multiplicative-expression :expr-kind)) additive-expression-multiplicative
         (eval (eval :multiplicative-expression)))
       (production (:additive-expression :expr-kind) ((:additive-expression any-value) + (:multiplicative-expression any-value)) additive-expression-add
         ((eval (e env))
          (letexc (left-value value ((eval :additive-expression) e))
            (letexc (right-value value ((eval :multiplicative-expression) e))
              (letexc (left-primitive value (coerce-to-primitive left-value (oneof no-hint)))
                (letexc (right-primitive value (coerce-to-primitive right-value (oneof no-hint)))
                  (if (or (is string-value left-primitive) (is string-value right-primitive))
                    (letexc (left-string string (coerce-to-string left-primitive))
                      (letexc (right-string string (coerce-to-string right-primitive))
                        (string-result (append left-string right-string))))
                    (apply-binary-float64-operator float64-add left-primitive right-primitive))))))))
       (production (:additive-expression :expr-kind) ((:additive-expression any-value) - (:multiplicative-expression any-value)) additive-expression-subtract
         ((eval (e env))
          (letexc (left-value value ((eval :additive-expression) e))
            (letexc (right-value value ((eval :multiplicative-expression) e))
              (apply-binary-float64-operator float64-subtract left-value right-value)))))
       (%print-actions)
       
       (%heading 1 "Bitwise Shift Operators")
       
       (declare-action eval (:shift-expression :expr-kind) (-> (env) value-or-exception))
       (production (:shift-expression :expr-kind) ((:additive-expression :expr-kind)) shift-expression-additive
         (eval (eval :additive-expression)))
       (production (:shift-expression :expr-kind) ((:shift-expression any-value) << (:additive-expression any-value)) shift-expression-left
         ((eval (e env))
          (letexc (bitmap-value value ((eval :shift-expression) e))
            (letexc (count-value value ((eval :additive-expression) e))
              (letexc (bitmap integer (coerce-to-uint32 bitmap-value))
                (letexc (count integer (coerce-to-uint32 count-value))
                  (integer-result (uint32-to-int32 (bitwise-and (bitwise-shift bitmap (bitwise-and count #x1F))
                                                                #xFFFFFFFF)))))))))
       (production (:shift-expression :expr-kind) ((:shift-expression any-value) >> (:additive-expression any-value)) shift-expression-right-signed
         ((eval (e env))
          (letexc (bitmap-value value ((eval :shift-expression) e))
            (letexc (count-value value ((eval :additive-expression) e))
              (letexc (bitmap integer (coerce-to-int32 bitmap-value))
                (letexc (count integer (coerce-to-uint32 count-value))
                  (integer-result (bitwise-shift bitmap (neg (bitwise-and count #x1F))))))))))
       (production (:shift-expression :expr-kind) ((:shift-expression any-value) >>> (:additive-expression any-value)) shift-expression-right-unsigned
         ((eval (e env))
          (letexc (bitmap-value value ((eval :shift-expression) e))
            (letexc (count-value value ((eval :additive-expression) e))
              (letexc (bitmap integer (coerce-to-uint32 bitmap-value))
                (letexc (count integer (coerce-to-uint32 count-value))
                  (integer-result (bitwise-shift bitmap (neg (bitwise-and count #x1F))))))))))
       (%print-actions)
       
       (%heading 1 "Relational Operators")
       
       (declare-action eval (:relational-expression :expr-kind) (-> (env) value-or-exception))
       (production (:relational-expression :expr-kind) ((:shift-expression :expr-kind)) relational-expression-shift
         (eval (eval :shift-expression)))
       (production (:relational-expression :expr-kind) ((:relational-expression any-value) < (:shift-expression any-value)) relational-expression-less
         ((eval (e env))
          (letexc (left-value value ((eval :relational-expression) e))
            (letexc (right-value value ((eval :shift-expression) e))
              (order-values left-value right-value true false)))))
       (production (:relational-expression :expr-kind) ((:relational-expression any-value) > (:shift-expression any-value)) relational-expression-greater
         ((eval (e env))
          (letexc (left-value value ((eval :relational-expression) e))
            (letexc (right-value value ((eval :shift-expression) e))
              (order-values right-value left-value true false)))))
       (production (:relational-expression :expr-kind) ((:relational-expression any-value) <= (:shift-expression any-value)) relational-expression-less-or-equal
         ((eval (e env))
          (letexc (left-value value ((eval :relational-expression) e))
            (letexc (right-value value ((eval :shift-expression) e))
              (order-values right-value left-value false true)))))
       (production (:relational-expression :expr-kind) ((:relational-expression any-value) >= (:shift-expression any-value)) relational-expression-greater-or-equal
         ((eval (e env))
          (letexc (left-value value ((eval :relational-expression) e))
            (letexc (right-value value ((eval :shift-expression) e))
              (order-values left-value right-value false true)))))
       (%print-actions)
       
       (define (order-values (left-value value) (right-value value) (less boolean) (greater-or-equal boolean)) value-or-exception
         (letexc (left-primitive value (coerce-to-primitive left-value (oneof number-hint)))
           (letexc (right-primitive value (coerce-to-primitive right-value (oneof number-hint)))
             (if (and (is string-value left-primitive) (is string-value right-primitive))
               (boolean-result
                (compare-strings (select string-value left-primitive) (select string-value right-primitive) less greater-or-equal greater-or-equal))
               (letexc (left-number float64 (coerce-to-float64 left-primitive))
                 (letexc (right-number float64 (coerce-to-float64 right-primitive))
                   (boolean-result (float64-compare left-number right-number less greater-or-equal greater-or-equal false))))))))
       
       (define (compare-strings (left string) (right string) (less boolean) (equal boolean) (greater boolean)) boolean
         (if (and (empty left) (empty right))
           equal
           (if (empty left)
             less
             (if (empty right)
               greater
               (let ((left-char-code integer (character-to-code (nth left 0)))
                     (right-char-code integer (character-to-code (nth right 0))))
                 (if (< left-char-code right-char-code)
                   less
                   (if (> left-char-code right-char-code)
                     greater
                     (compare-strings (subseq left 1) (subseq right 1) less equal greater))))))))
       
       (%heading 1 "Equality Operators")
       
       (declare-action eval (:equality-expression :expr-kind) (-> (env) value-or-exception))
       (production (:equality-expression :expr-kind) ((:relational-expression :expr-kind)) equality-expression-relational
         (eval (eval :relational-expression)))
       (production (:equality-expression :expr-kind) ((:equality-expression any-value) == (:relational-expression any-value)) equality-expression-equal
         ((eval (e env))
          (letexc (left-value value ((eval :equality-expression) e))
            (letexc (right-value value ((eval :relational-expression) e))
              (letexc (eq boolean (compare-values left-value right-value))
                (boolean-result eq))))))
       (production (:equality-expression :expr-kind) ((:equality-expression any-value) != (:relational-expression any-value)) equality-expression-not-equal
         ((eval (e env))
          (letexc (left-value value ((eval :equality-expression) e))
            (letexc (right-value value ((eval :relational-expression) e))
              (letexc (eq boolean (compare-values left-value right-value))
                (boolean-result (not eq)))))))
       (production (:equality-expression :expr-kind) ((:equality-expression any-value) === (:relational-expression any-value)) equality-expression-strict-equal
         ((eval (e env))
          (letexc (left-value value ((eval :equality-expression) e))
            (letexc (right-value value ((eval :relational-expression) e))
              (boolean-result (strict-compare-values left-value right-value))))))
       (production (:equality-expression :expr-kind) ((:equality-expression any-value) !== (:relational-expression any-value)) equality-expression-strict-not-equal
         ((eval (e env))
          (letexc (left-value value ((eval :equality-expression) e))
            (letexc (right-value value ((eval :relational-expression) e))
              (boolean-result (not (strict-compare-values left-value right-value)))))))
       (%print-actions)
       
       (define (compare-values (left-value value) (right-value value)) boolean-or-exception
         (case left-value
           (((undefined-value null-value))
            (case right-value
              (((undefined-value null-value)) (oneof normal true))
              (((boolean-value number-value string-value object-value)) (oneof normal false))))
           ((boolean-value left-bool boolean)
            (case right-value
              (((undefined-value null-value)) (oneof normal false))
              ((boolean-value right-bool boolean) (oneof normal (not (xor left-bool right-bool))))
              (((number-value string-value object-value))
               (compare-float64-to-value (coerce-boolean-to-float64 left-bool) right-value))))
           ((number-value left-number float64)
            (compare-float64-to-value left-number right-value))
           ((string-value left-str string)
            (case right-value
              (((undefined-value null-value)) (oneof normal false))
              ((boolean-value right-bool boolean)
               (letexc (left-number float64 (coerce-to-float64 left-value))
                 (oneof normal (float64-equal left-number (coerce-boolean-to-float64 right-bool)))))
              ((number-value right-number float64)
               (letexc (left-number float64 (coerce-to-float64 left-value))
                 (oneof normal (float64-equal left-number right-number))))
              ((string-value right-str string)
               (oneof normal (compare-strings left-str right-str false true false)))
              (object-value
               (letexc (right-primitive value (coerce-to-primitive right-value (oneof no-hint)))
                 (compare-values left-value right-primitive)))))
           ((object-value left-obj object)
            (case right-value
              (((undefined-value null-value)) (oneof normal false))
              ((boolean-value right-bool boolean)
               (letexc (left-primitive value (coerce-to-primitive left-value (oneof no-hint)))
                 (compare-values left-primitive (oneof number-value (coerce-boolean-to-float64 right-bool)))))
              (((number-value string-value))
               (letexc (left-primitive value (coerce-to-primitive left-value (oneof no-hint)))
                 (compare-values left-primitive right-value)))
              ((object-value right-obj object)
               (oneof normal (address-equal (& properties left-obj) (& properties right-obj))))))))
       
       (define (compare-float64-to-value (left-number float64) (right-value value)) boolean-or-exception
         (case right-value
           (((undefined-value null-value)) (oneof normal false))
           (((boolean-value number-value string-value))
            (letexc (right-number float64 (coerce-to-float64 right-value))
              (oneof normal (float64-equal left-number right-number))))
           (object-value
            (letexc (right-primitive value (coerce-to-primitive right-value (oneof no-hint)))
              (compare-float64-to-value left-number right-primitive)))))
       
       (define (float64-equal (x float64) (y float64)) boolean
         (float64-compare x y false true false false))
       
       (define (strict-compare-values (left-value value) (right-value value)) boolean
         (case left-value
           (undefined-value
            (is undefined-value right-value))
           (null-value
            (is null-value right-value))
           ((boolean-value left-bool boolean)
            (case right-value
              ((boolean-value right-bool boolean) (not (xor left-bool right-bool)))
              (((undefined-value null-value number-value string-value object-value)) false)))
           ((number-value left-number float64)
            (case right-value
              ((number-value right-number float64) (float64-equal left-number right-number))
              (((undefined-value null-value boolean-value string-value object-value)) false)))
           ((string-value left-str string)
            (case right-value
              ((string-value right-str string)
               (compare-strings left-str right-str false true false))
              (((undefined-value null-value boolean-value number-value object-value)) false)))
           ((object-value left-obj object)
            (case right-value
              ((object-value right-obj object)
               (address-equal (& properties left-obj) (& properties right-obj)))
              (((undefined-value null-value boolean-value number-value string-value)) false)))))
       
       (%heading 1 "Binary Bitwise Operators")
       
       (declare-action eval (:bitwise-and-expression :expr-kind) (-> (env) value-or-exception))
       (production (:bitwise-and-expression :expr-kind) ((:equality-expression :expr-kind)) bitwise-and-expression-equality
         (eval (eval :equality-expression)))
       (production (:bitwise-and-expression :expr-kind) ((:bitwise-and-expression any-value) & (:equality-expression any-value)) bitwise-and-expression-and
         ((eval (e env))
          (letexc (left-value value ((eval :bitwise-and-expression) e))
            (letexc (right-value value ((eval :equality-expression) e))
              (apply-binary-bitwise-operator bitwise-and left-value right-value)))))
       
       (declare-action eval (:bitwise-xor-expression :expr-kind) (-> (env) value-or-exception))
       (production (:bitwise-xor-expression :expr-kind) ((:bitwise-and-expression :expr-kind)) bitwise-xor-expression-bitwise-and
         (eval (eval :bitwise-and-expression)))
       (production (:bitwise-xor-expression :expr-kind) ((:bitwise-xor-expression any-value) ^ (:bitwise-and-expression any-value)) bitwise-xor-expression-xor
         ((eval (e env))
          (letexc (left-value value ((eval :bitwise-xor-expression) e))
            (letexc (right-value value ((eval :bitwise-and-expression) e))
              (apply-binary-bitwise-operator bitwise-xor left-value right-value)))))
       
       (declare-action eval (:bitwise-or-expression :expr-kind) (-> (env) value-or-exception))
       (production (:bitwise-or-expression :expr-kind) ((:bitwise-xor-expression :expr-kind)) bitwise-or-expression-bitwise-xor
         (eval (eval :bitwise-xor-expression)))
       (production (:bitwise-or-expression :expr-kind) ((:bitwise-or-expression any-value) \| (:bitwise-xor-expression any-value)) bitwise-or-expression-or
         ((eval (e env))
          (letexc (left-value value ((eval :bitwise-or-expression) e))
            (letexc (right-value value ((eval :bitwise-xor-expression) e))
              (apply-binary-bitwise-operator bitwise-or left-value right-value)))))
       (%print-actions)
       
       (define (apply-binary-bitwise-operator (operator (-> (integer integer) integer)) (left-value value) (right-value value)) value-or-exception
         (letexc (left-int integer (coerce-to-int32 left-value))
           (letexc (right-int integer (coerce-to-int32 right-value))
             (integer-result (operator left-int right-int)))))
       
       (%heading 1 "Binary Logical Operators")
       
       (declare-action eval (:logical-and-expression :expr-kind) (-> (env) value-or-exception))
       (production (:logical-and-expression :expr-kind) ((:bitwise-or-expression :expr-kind)) logical-and-expression-bitwise-or
         (eval (eval :bitwise-or-expression)))
       (production (:logical-and-expression :expr-kind) ((:logical-and-expression any-value) && (:bitwise-or-expression any-value)) logical-and-expression-and
         ((eval (e env))
          (letexc (left-value value ((eval :logical-and-expression) e))
            (if (coerce-to-boolean left-value)
              ((eval :bitwise-or-expression) e)
              (oneof normal left-value)))))
       
       (declare-action eval (:logical-or-expression :expr-kind) (-> (env) value-or-exception))
       (production (:logical-or-expression :expr-kind) ((:logical-and-expression :expr-kind)) logical-or-expression-logical-and
         (eval (eval :logical-and-expression)))
       (production (:logical-or-expression :expr-kind) ((:logical-or-expression any-value) \|\| (:logical-and-expression any-value)) logical-or-expression-or
         ((eval (e env))
          (letexc (left-value value ((eval :logical-or-expression) e))
            (if (coerce-to-boolean left-value)
              (oneof normal left-value)
              ((eval :logical-and-expression) e)))))
       (%print-actions)
       
       (%heading 1 "Conditional Operator")
       
       (declare-action eval (:conditional-expression :expr-kind) (-> (env) value-or-exception))
       (production (:conditional-expression :expr-kind) ((:logical-or-expression :expr-kind)) conditional-expression-logical-or
         (eval (eval :logical-or-expression)))
       (production (:conditional-expression :expr-kind) ((:logical-or-expression any-value) ? (:assignment-expression any-value) \: (:assignment-expression any-value)) conditional-expression-conditional
         ((eval (e env))
          (letexc (condition value ((eval :logical-or-expression) e))
            (if (coerce-to-boolean condition)
              ((eval :assignment-expression 1) e)
              ((eval :assignment-expression 2) e)))))
       (%print-actions)
       
       (%heading 1 "Assignment Operators")
       
       (declare-action eval (:assignment-expression :expr-kind) (-> (env) value-or-exception))
       (production (:assignment-expression :expr-kind) ((:conditional-expression :expr-kind)) assignment-expression-conditional
         (eval (eval :conditional-expression)))
       (production (:assignment-expression :expr-kind) (:lvalue = (:assignment-expression any-value)) assignment-expression-assignment
         ((eval (e env))
          (letexc (left-reference reference ((eval :lvalue) e))
            (letexc (right-value value ((eval :assignment-expression) e))
              (letexc (u void (reference-put-value left-reference right-value) :unused)
                (oneof normal right-value))))))
       #|
       (production (:assignment-expression :expr-kind) (:lvalue :compound-assignment (:assignment-expression any-value)) assignment-expression-compound-assignment
         ((eval (e env))
          (letexc (left-reference reference ((eval :lvalue) e))
            (letexc (left-value value (reference-get-value left-reference))
              (letexc (right-value value ((eval :assignment-expression) e))
                (letexc (res-value ((compound-operator :compound-assignment) left-value right-value))
                  (letexc (u void (reference-put-value left-reference res-value) :unused)
                    (oneof normal res-value))))))))
       
       (declare-action compound-operator :compound-assignment (-> (value value) value-or-exception))
       (production :compound-assignment (*=) compound-assignment-multiply
         (compound-operator (binary-float64-compound-operator float64-multiply)))
       (production :compound-assignment (/=) compound-assignment-divide
         (compound-operator (binary-float64-compound-operator float64-divide)))
       (production :compound-assignment (%=) compound-assignment-remainder
         (compound-operator (binary-float64-compound-operator float64-remainder)))
       (production :compound-assignment (+=) compound-assignment-add
         (compound-operator (binary-float64-compound-operator float64-remainder)))
       (production :compound-assignment (-=) compound-assignment-subtract
         (compound-operator (binary-float64-compound-operator float64-subtract)))
       (%print-actions)
       
       (define (binary-float64-compound-operator (operator (-> (float64 float64) float64))) (-> (value value) value-or-exception)
         (function ((left-value value) (right-value value))
           (letexc (left-number float64 (coerce-to-float64 left-value))
             (letexc (right-number float64 (coerce-to-float64 right-value))
               (oneof normal (oneof number-value (operator left-number right-number)))))))
       |#
       (%heading 1 "Expressions")
       
       (declare-action eval (:comma-expression :expr-kind) (-> (env) value-or-exception))
       (production (:comma-expression :expr-kind) ((:assignment-expression :expr-kind)) comma-expression-assignment
         (eval (eval :assignment-expression)))
       (%print-actions)
       
       (declare-action eval :expression (-> (env) value-or-exception))
       (production :expression ((:comma-expression any-value)) expression-comma-expression
         (eval (eval :comma-expression)))
       (%print-actions)
       
       (%heading 1 "Programs")
       
       (declare-action eval :program value-or-exception)
       (production :program (:expression $end) program
         (eval ((eval :expression) (tuple env (oneof null-object-or-null)))))
       )))
  
  (defparameter *gg* (world-grammar *gw* 'code-grammar)))


(defun token-terminal (token)
  (if (symbolp token)
    token
    (car token)))

(defun ecma-parse-tokens (tokens &key trace)
  (action-parse *gg* #'token-terminal tokens :trace trace))


(defun ecma-parse (string &key trace)
  (let ((tokens (tokenize string)))
    (when trace
      (format *trace-output* "~S~%" tokens))
    (action-parse *gg* #'token-terminal tokens :trace trace)))


; Same as ecma-parse except that also print the action results nicely.
(defun ecma-pparse (string &key (stream t) trace)
  (multiple-value-bind (results types) (ecma-parse string :trace trace)
    (print-values results types stream)
    (terpri stream)
    (values results types)))


#|
(depict-rtf-to-local-file
 "JSECMA/ParserSemantics.rtf"
 "ECMAScript 1 Parser Semantics"
 #'(lambda (rtf-stream)
     (depict-world-commands rtf-stream *gw* :heading-offset 1)))

(depict-html-to-local-file
 "JSECMA/ParserSemantics.html"
 "ECMAScript 1 Parser Semantics"
 t
 #'(lambda (html-stream)
     (depict-world-commands html-stream *gw* :heading-offset 1)))

(with-local-output (s "JSECMA/ParserGrammar.txt") (print-grammar *gg* s))


(ecma-pparse "('abc')")
(ecma-pparse "!~ 352")
(ecma-pparse "1e308%.125")
(ecma-pparse "-3>>>10-6")
(ecma-pparse "-3>>0")
(ecma-pparse "1+2*3|16")
(ecma-pparse "1==true")
(ecma-pparse "1=true")
(ecma-pparse "x=true")
(ecma-pparse "2*4+17+0x32")
(ecma-pparse "+'ab'+'de'")
|#
