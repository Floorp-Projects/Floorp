;;; The contents of this file are subject to the Netscape Public License
;;; Version 1.0 (the "NPL"); you may not use this file except in
;;; compliance with the NPL.  You may obtain a copy of the NPL at
;;; http://www.mozilla.org/NPL/
;;;
;;; Software distributed under the NPL is distributed on an "AS IS" basis,
;;; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
;;; for the specific language governing rights and limitations under the
;;; NPL.
;;;
;;; The Initial Developer of this code under the NPL is Netscape
;;; Communications Corporation.  Portions created by Netscape are
;;; Copyright (C) 1998 Netscape Communications Corporation.  All Rights
;;; Reserved.

;;;
;;; ECMAScript semantic calculus markup emitters
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;


;;; ------------------------------------------------------------------------------------------------------
;;; SEMANTIC DEPICTION UTILITIES

(defparameter *semantic-keywords*
  '(not and or is type oneof tuple action lambda if then else in new case of end let letexc))

; Emit markup for one of the semantic keywords, as specified by keyword-symbol.
(defun depict-semantic-keyword (markup-stream keyword-symbol)
  (assert-true (find keyword-symbol *semantic-keywords* :test #'eq))
  (depict-char-style (markup-stream :semantic-keyword)
    (depict markup-stream (string-downcase (symbol-name keyword-symbol)))))


; If test is true, depict an opening parenthesis, evaluate body, and depict a closing
; parentheses.  Otherwise, just evaluate body.
; Return the result value of body.
(defmacro depict-optional-parentheses ((markup-stream test) &body body)
  (let ((temp (gensym "PAREN")))
    `(let ((,temp ,test))
       (when ,temp
         (depict ,markup-stream "("))
       (prog1
         (progn ,@body)
         (when ,temp
           (depict ,markup-stream ")"))))))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICT-ENV

; A depict-env holds state that helps in depicting a grammar or lexer.
(defstruct depict-env
  (grammar-info nil :type (or null grammar-info))                ;The current grammar-info or nil if none
  (seen-nonterminals nil :type (or null hash-table))             ;Hash table (nonterminal -> t) of nonterminals already depicted
  (seen-grammar-arguments nil :type (or null hash-table))        ;Hash table (grammar-argument -> t) of grammar-arguments already depicted
  (mode nil :type (member nil :syntax :semantics))               ;Current heading (:syntax or :semantics) or nil if none
  (pending-actions-reverse nil :type list))                      ;Reverse-order list of closures of actions pending for a %print-actions


(defun checked-depict-env-grammar-info (depict-env)
  (or (depict-env-grammar-info depict-env)
      (error "Grammar needed")))


(defvar *visible-modes* t)

; Set the mode to the given mode, emitting a heading if necessary.
(defun depict-mode (markup-stream depict-env mode)
  (unless (eq mode (depict-env-mode depict-env))
    (when *visible-modes*
      (ecase mode
        (:syntax (depict-paragraph (markup-stream ':grammar-header)
                   (depict markup-stream "Syntax")))
        (:semantics (depict-paragraph (markup-stream ':grammar-header)
                      (depict markup-stream "Semantics")))
        ((nil))))
    (setf (depict-env-mode depict-env) mode)))


; Emit markup paragraphs for a command.
(defun depict-command (markup-stream world depict-env command)
  (handler-bind ((error #'(lambda (condition)
                            (declare (ignore condition))
                            (format *error-output* "~&While depicting: ~:W~%" command))))
    (let ((depictor (and (consp command)
                         (identifier? (first command))
                         (get (world-intern world (first command)) :depict-command))))
      (if depictor
        (apply depictor markup-stream world depict-env (rest command))
        (error "Bad command: ~S" command)))))


; Emit markup paragraphs for the world's commands.
(defun depict-world-commands (markup-stream world)
  (let ((depict-env (make-depict-env)))
    (dolist (command (world-commands-source world))
      (depict-command markup-stream world depict-env command))
    (depict-clear-grammar markup-stream world depict-env)))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING TYPES

(defconstant *type-level-min* 0)
(defconstant *type-level-suffix* 1)
(defconstant *type-level-function* 2)
(defconstant *type-level-max* 2)
;;;
;;; The level argument indicates what kinds of component types may be represented without being placed
;;; in parentheses.
;;;  level    kinds
;;;    0      id, oneof, tuple, (type)
;;;    1      id, oneof, tuple, (type), type[], type^
;;;    2      id, oneof, tuple, (type), type[], type^, type x type -> type


; Emit markup for the name of a type, which must be a symbol.
(defun depict-type-name (markup-stream type-name)
  (depict-char-style (markup-stream :type-name)
    (depict markup-stream (symbol-upper-mixed-case-name type-name))))


; Emit markup for the name of a tuple or oneof field, which must be a symbol.
(defun depict-field-name (markup-stream field-name)
  (depict-char-style (markup-stream :field-name)
    (depict markup-stream (symbol-lower-mixed-case-name field-name))))


; If level < threshold, depict an opening parenthesis, evaluate body, and depict a closing
; parentheses.  Otherwise, just evaluate body.
; Return the result value of body.
(defmacro depict-type-parentheses ((markup-stream level threshold) &body body)
  `(depict-optional-parentheses (,markup-stream (< ,level ,threshold))
     ,@body))


; Emit markup for the given type expression.  level is non-nil if this is a recursive
; call to depict-type-expr for which the markup-stream's style is :type-expression.
; In this case level indicates the binding level imposed by the enclosing type expression.
(defun depict-type-expr (markup-stream world type-expr &optional level)
  (cond
   ((identifier? type-expr)
    (depict-type-name markup-stream type-expr))
   ((type? type-expr)
    (let ((type-str (print-type-to-string type-expr)))
      (warn "Depicting raw type ~A" type-str)
      (depict markup-stream "<<<" type-str ">>>")))
   (t (let ((depictor (get (world-intern world (first type-expr)) :depict-type-constructor)))
        (if level
          (apply depictor markup-stream world level (rest type-expr))
          (depict-char-style (markup-stream :type-expression)
            (apply depictor markup-stream world *type-level-max* (rest type-expr))))))))


; (-> (<arg-type1> ... <arg-typen>) <result-type>)
; Level 2
;   "<arg-type1>@1 x ... x <arg-typen>@1 -> <result-type>@1"
(defun depict--> (markup-stream world level arg-type-exprs result-type-expr)
  (depict-type-parentheses (markup-stream level *type-level-function*)
    (depict-list markup-stream
                 #'(lambda (markup-stream arg-type-expr)
                     (depict-type-expr markup-stream world arg-type-expr *type-level-suffix*))
                 arg-type-exprs
                 :separator '(" " :cartesian-product-10 " ")
                 :empty "()")
    (depict markup-stream " " :function-arrow-10 " ")
    (depict-type-expr markup-stream world result-type-expr *type-level-suffix*)))


; (vector <element-type>)
; Level 1
;   "<element-type>@1[]"
(defun depict-vector (markup-stream world level element-type-expr)
  (depict-type-parentheses (markup-stream level *type-level-suffix*)
    (depict-type-expr markup-stream world element-type-expr *type-level-suffix*)
    (depict markup-stream "[]")))


; (address <element-type>)
; Level 1
;   "<element-type>@1^"
(defun depict-address (markup-stream world level element-type-expr)
  (depict-type-parentheses (markup-stream level *type-level-suffix*)
    (depict-type-expr markup-stream world element-type-expr *type-level-suffix*)
    (depict markup-stream :up-arrow-10)))


(defun depict-tuple-or-oneof (markup-stream world keyword-symbol tag-pairs)
  (depict-semantic-keyword markup-stream keyword-symbol)
  (depict-list
   markup-stream
   #'(lambda (markup-stream tag-pair)
       (if (identifier? tag-pair)
         (depict-field-name markup-stream tag-pair)
         (progn
           (depict-field-name markup-stream (first tag-pair))
           (depict markup-stream ": ")
           (depict-type-expr markup-stream world (second tag-pair) *type-level-function*))))
   tag-pairs
   :indent 6
   :prefix " {"
   :prefix-break 0
   :suffix "}"
   :separator ";"
   :break 1
   :empty nil))

; (oneof (<tag1> <type1>) ... (<tagn> <typen>))
; Level 0
;   "ONEOF{<tag1>: <type1>@0; ...; <tagn>:<typen>@0}"
(defun depict-oneof (markup-stream world level &rest tags-and-types)
  (declare (ignore level))
  (depict-tuple-or-oneof markup-stream world 'oneof tags-and-types))

; (tuple (<tag1> <type1>) ... (<tagn> <typen>))
; Level 0
;   "TUPLE{<tag1>: <type1>@0; ...; <tagn>:<typen>@0}"
(defun depict-tuple (markup-stream world level &rest tags-and-types)
  (declare (ignore level))
  (depict-tuple-or-oneof markup-stream world 'tuple tags-and-types))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING EXPRESSIONS


(defconstant *primitive-level-min* 0)
(defconstant *primitive-level-unary-suffix* 1)
(defconstant *primitive-level-unary-prefix* 2)
(defconstant *primitive-level-unary* 3)
(defconstant *primitive-level-multiplicative* 4)
(defconstant *primitive-level-additive* 5)
(defconstant *primitive-level-relational* 6)
(defconstant *primitive-level-logical* 7)
(defconstant *primitive-level-unparenthesized-new* 8)
(defconstant *primitive-level-expr* 9)
(defconstant *primitive-level-stmt* 10)
(defconstant *primitive-level-max* 10)
;;;
;;; The level argument indicates what kinds of subexpressions may be represented without being placed
;;; in parentheses (or on a separate line for the case of lambda and if/then/else).
;;;  level    kinds
;;;    0      id, constant, (e)
;;;    1      id, constant, (e), f(...), new(v), a[i]
;;;    2      id, constant, (e), -e, @
;;;    3      id, constant, (e), f(...), new(v), a[i], -e, @
;;;    4      id, constant, (e), f(...), new(v), a[i], -e, @, /, *
;;;    5      id, constant, (e), f(...), new(v), a[i], -e, @, /, *, +, -
;;;    6      id, constant, (e), f(...), new(v), a[i], -e, @, /, *, +, -, relationals
;;;    7      id, constant, (e), f(...), new(v), a[i], -e, @, /, *, +, -, relationals, logicals
;;;    8      id, constant, (e), f(...), new(v), a[i], -e, @, /, *, +, -, relationals, logicals, new v
;;;    9      id, constant, (e), f(...), new(v), a[i], -e, @, /, *, +, -, relationals, logicals, new v
;;;   10      id, constant, (e), f(...), new(v), a[i], -e, @, /, *, +, -, relationals, logicals, new v, :=, lambda, if/then/else

; Return true if primitive-level1 is a superset of primitive-level2
; in the partial order of primitive levels.
(defun primitive-level->= (primitive-level1 primitive-level2)
  (and (>= primitive-level1 primitive-level2)
       (or (/= primitive-level1 *primitive-level-unary-prefix*)
           (/= primitive-level2 *primitive-level-unary-suffix*))))


; If primitive-level is not a superset of threshold, depict an opening parenthesis,
; evaluate body, and depict a closing parentheses.  Otherwise, just evaluate body.
; Return the result value of body.
(defmacro depict-expr-parentheses ((markup-stream primitive-level threshold) &body body)
  `(depict-optional-parentheses (,markup-stream (not (primitive-level->= ,primitive-level ,threshold)))
     ,@body))


; Emit markup for the name of a global variable, which must be a symbol.
(defun depict-global-variable (markup-stream name)
  (depict-char-style (markup-stream :global-variable)
    (depict markup-stream (symbol-lower-mixed-case-name name))))


; Emit markup for the name of a local variable, which must be a symbol.
(defun depict-local-variable (markup-stream name)
  (depict-char-style (markup-stream :local-variable)
    (depict markup-stream (symbol-lower-mixed-case-name name))))


; Emit markup for the name of an action, which must be a symbol.
(defun depict-action-name (markup-stream action-name)
  (depict-char-style (markup-stream :action-name)
    (depict markup-stream (symbol-upper-mixed-case-name action-name))))


; Emit markup for the value constant.
(defun depict-constant (markup-stream constant)
  (cond
   ((integerp constant)
    (depict-integer markup-stream constant))
   ((floatp constant)
    (depict markup-stream (format nil (if (= constant (floor constant 1)) "~,1F" "~F") constant)))
   ((characterp constant)
    (depict markup-stream ':left-single-quote)
    (depict-char-style (markup-stream ':character-literal)
      (depict-character markup-stream constant nil))
    (depict markup-stream ':right-single-quote))
   ((stringp constant)
    (depict-string markup-stream constant))
   (t (error "Bad constant ~S" constant))))


; Emit markup for the primitive when it is not called in a function call.
(defun depict-primitive (markup-stream primitive)
  (unless (eq (primitive-appearance primitive) ':global)
    (error "Can't depict primitive ~S outside a call" primitive))
  (depict-item-or-group-list markup-stream (primitive-markup1 primitive)))


; Emit markup for the parameters to a function call.
(defun depict-call-parameters (markup-stream world annotated-parameters)
  (depict-list markup-stream
               #'(lambda (markup-stream parameter)
                   (depict-annotated-value-expr markup-stream world parameter))
               annotated-parameters
               :indent 4
               :prefix "("
               :prefix-break 0
               :suffix ")"
               :separator ","
               :break 1
               :empty nil))


; Emit markup for the function or primitive call.  level indicates the binding level imposed
; by the enclosing expression.
(defun depict-call (markup-stream world level annotated-function-expr &rest annotated-arg-exprs)
  (if (eq (first annotated-function-expr) 'expr-annotation:primitive)
    (let ((primitive (symbol-primitive (second annotated-function-expr))))
      (depict-expr-parentheses (markup-stream level (primitive-level primitive))
        (ecase (primitive-appearance primitive)
          (:global
           (depict-primitive markup-stream primitive)
           (depict-call-parameters markup-stream world annotated-arg-exprs))
          (:infix
           (assert-true (= (length annotated-arg-exprs) 2))
           (depict-logical-block (markup-stream 0)
             (depict-annotated-value-expr markup-stream world (first annotated-arg-exprs) (primitive-level1 primitive))
             (let ((spaces (primitive-markup2 primitive)))
               (when spaces
                 (depict-space markup-stream))
               (depict-item-or-group-list markup-stream (primitive-markup1 primitive))
               (depict-break markup-stream (if spaces 1 0)))
             (depict-annotated-value-expr markup-stream world (second annotated-arg-exprs) (primitive-level2 primitive))))
          (:unary
           (assert-true (= (length annotated-arg-exprs) 1))
           (depict-item-or-group-list markup-stream (primitive-markup1 primitive))
           (depict-annotated-value-expr markup-stream world (first annotated-arg-exprs) (primitive-level1 primitive))
           (depict-item-or-group-list markup-stream (primitive-markup2 primitive)))
          (:phantom
           (assert-true (= (length annotated-arg-exprs) 1))
           (depict-annotated-value-expr markup-stream world (first annotated-arg-exprs) level)))))
    (depict-expr-parentheses (markup-stream level *primitive-level-unary-suffix*)
      (depict-annotated-value-expr markup-stream world annotated-function-expr *primitive-level-unary-suffix*)
      (depict-call-parameters markup-stream world annotated-arg-exprs))))


; Emit markup for the reference to the action on the given general grammar symbol.
(defun depict-action-reference (markup-stream action-name general-grammar-symbol &optional index)
  (depict-action-name markup-stream action-name)
  (depict markup-stream :action-begin)
  (depict-general-grammar-symbol markup-stream general-grammar-symbol index)
  (depict markup-stream :action-end))


; Emit markup for the given annotated value expression.  level indicates the binding level imposed
; by the enclosing expression.
(defun depict-annotated-value-expr (markup-stream world annotated-expr &optional (level *primitive-level-expr*))
  (let ((annotation (first annotated-expr))
        (args (rest annotated-expr)))
    (ecase annotation
      (expr-annotation:constant (depict-constant markup-stream (first args)))
      (expr-annotation:primitive (depict-primitive markup-stream (symbol-primitive (first args))))
      (expr-annotation:local (depict-local-variable markup-stream (first args)))
      (expr-annotation:global (depict-global-variable markup-stream (first args)))
      (expr-annotation:call (apply #'depict-call markup-stream world level args))
      (expr-annotation:action (apply #'depict-action-reference markup-stream args))
      (expr-annotation:special-form
       (apply (get (first args) :depict-special-form) markup-stream world level (rest args)))
      (expr-annotation:macro
       (let ((depictor (get (first args) :depict-macro)))
         (if depictor
           (apply depictor markup-stream world level (rest args))
           (depict-annotated-value-expr markup-stream world (second args) level)))))))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING SPECIAL FORMS

(defmacro depict-statement ((markup-stream keyword &optional (space t)) &body body)
  `(depict-logical-block (,markup-stream 0)
     (when (< level *primitive-level-stmt*)
       (depict-break ,markup-stream))
     (depict-expr-parentheses (,markup-stream level *primitive-level-stmt*)
       (depict-semantic-keyword ,markup-stream ,keyword)
       ,@(and space `((depict-space ,markup-stream)))
       ,@body)))


; (bottom <type>)
(defun depict-bottom (markup-stream world level type-expr)
  (declare (ignore world level type-expr))
  (depict markup-stream ':bottom-10))


(defun depict-lambda-bindings (markup-stream world arg-binding-exprs)
  (depict-list markup-stream
               #'(lambda (markup-stream arg-binding)
                   (depict-local-variable markup-stream (first arg-binding))
                   (depict markup-stream ": ")
                   (depict-type-expr markup-stream world (second arg-binding)))
               arg-binding-exprs
               :prefix "("
               :suffix ")"
               :separator ", "
               :empty nil))

; (lambda ((<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <body>)
(defun depict-lambda (markup-stream world level arg-binding-exprs body-annotated-expr)
  (depict-statement (markup-stream 'lambda nil)
    (depict-lambda-bindings markup-stream world arg-binding-exprs)
    (depict-logical-block (markup-stream 4)
      (depict-break markup-stream)
      (depict-annotated-value-expr markup-stream world body-annotated-expr *primitive-level-stmt*))))


; (if <condition-expr> <true-expr> <false-expr>)
(defun depict-if (markup-stream world level condition-annotated-expr true-annotated-expr false-annotated-expr)
  (depict-statement (markup-stream 'if)
    (depict-logical-block (markup-stream 4)
      (depict-annotated-value-expr markup-stream world condition-annotated-expr))
    (depict-break markup-stream)
    (depict-semantic-keyword markup-stream 'then)
    (depict-space markup-stream)
    (depict-logical-block (markup-stream 7)
      (depict-annotated-value-expr markup-stream world true-annotated-expr *primitive-level-stmt*))
    (depict-break markup-stream)
    (depict-semantic-keyword markup-stream 'else)
    (depict-space markup-stream)
    (depict-logical-block (markup-stream (if (special-form-annotated-expr? 'if false-annotated-expr) nil 6))
      (depict-annotated-value-expr markup-stream world false-annotated-expr *primitive-level-stmt*))))


;;; Vectors

; (vector <element-expr> <element-expr> ... <element-expr>)
(defun depict-vector-form (markup-stream world level &rest element-annotated-exprs)
  (declare (ignore level))
  (depict-list markup-stream
               #'(lambda (markup-stream element-annotated-expr)
                   (depict-annotated-value-expr markup-stream world element-annotated-expr))
               element-annotated-exprs
               :indent 1
               :prefix ':vector-begin
               :suffix ':vector-end
               :separator ","
               :break 1))


(defun depict-subscript-type-expr (markup-stream world type-expr)
  (depict-char-style (markup-stream 'sub)
    (depict-type-expr markup-stream world type-expr)))


; (vector-of <element-type>)
(defun depict-vector-of (markup-stream world level element-type-expr)
  (declare (ignore level))
  (depict markup-stream ':empty-vector)
  (depict-subscript-type-expr markup-stream world element-type-expr))


(defun depict-special-function (markup-stream world name-str &rest arg-annotated-exprs)
  (depict-char-style (markup-stream :global-variable)
    (depict markup-stream name-str))
  (depict-call-parameters markup-stream world arg-annotated-exprs))


; (empty <vector-expr>)
(defun depict-empty (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict-special-function markup-stream world "empty" vector-annotated-expr))


; (length <vector-expr>)
(defun depict-length (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict-special-function markup-stream world "length" vector-annotated-expr))


; (first <vector-expr>)
(defun depict-first (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict-special-function markup-stream world "first" vector-annotated-expr))


; (last <vector-expr>)
(defun depict-last (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict-special-function markup-stream world "last" vector-annotated-expr))


; (rest <vector-expr>)
(defun depict-rest (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict-special-function markup-stream world "rest" vector-annotated-expr))


; (butlast <vector-expr>)
(defun depict-butlast (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict-special-function markup-stream world "butLast" vector-annotated-expr))


; (nth <vector-expr> <n-expr>)
(defun depict-nth (markup-stream world level vector-annotated-expr n-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-unary-suffix*)
    (depict-annotated-value-expr markup-stream world vector-annotated-expr *primitive-level-unary-suffix*)
    (depict markup-stream "[")
    (depict-annotated-value-expr markup-stream world n-annotated-expr)
    (depict markup-stream "]")))


; (append <vector-expr> <vector-expr>)
(defun depict-append (markup-stream world level vector1-annotated-expr vector2-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-additive*)
    (depict-logical-block (markup-stream 0)
      (depict-annotated-value-expr markup-stream world vector1-annotated-expr *primitive-level-additive*)
      (depict markup-stream " " :vector-append)
      (depict-break markup-stream 1)
      (depict-annotated-value-expr markup-stream world vector2-annotated-expr *primitive-level-additive*))))


;;; Oneofs

; (oneof <oneof-type> <tag> <value-expr>)
(defun depict-oneof-form (markup-stream world level tag &optional value-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-unary-prefix*)
    (depict-field-name markup-stream tag)
    (when value-annotated-expr
      (depict-logical-block (markup-stream 4)
        (depict-break markup-stream 1)
        (depict-annotated-value-expr markup-stream world value-annotated-expr *primitive-level-unary*)))))


; (typed-oneof <type-expr> <tag> <value-expr>)
(defun depict-typed-oneof (markup-stream world level type-expr tag &optional value-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-unary-prefix*)
    (depict-field-name markup-stream tag)
    (depict-subscript-type-expr markup-stream world type-expr)
    (when value-annotated-expr
      (depict-logical-block (markup-stream 4)
        (depict-break markup-stream 1)
        (depict-annotated-value-expr markup-stream world value-annotated-expr *primitive-level-unary*)))))


; (case <oneof-expr> (<tag-spec> <value-expr>) (<tag-spec> <value-expr>) ... (<tag-spec> <value-expr>))
; where each <tag-spec> is either ((<tag> <tag> ... <tag>) nil nil) or ((<tag>) <var> <type>)
(defun depict-case (markup-stream world level oneof-annotated-expr &rest annotated-cases)
  (depict-statement (markup-stream 'case)
    (depict-logical-block (markup-stream 6)
      (depict-annotated-value-expr markup-stream world oneof-annotated-expr))
    (depict-space markup-stream)
    (depict-semantic-keyword markup-stream 'of)
    (depict-logical-block (markup-stream 3)
      (mapl #'(lambda (annotated-cases)
                (let* ((annotated-case (first annotated-cases))
                       (tag-spec (first annotated-case))
                       (tags (first tag-spec))
                       (var (second tag-spec))
                       (value-annotated-expr (second annotated-case)))
                  (depict-break markup-stream)
                  (depict-logical-block (markup-stream 6)
                    (depict-list markup-stream
                                 #'depict-field-name
                                 tags
                                 :indent 0
                                 :separator ","
                                 :break 1)
                    (when var
                      (depict markup-stream "(")
                      (depict-local-variable markup-stream var)
                      (depict markup-stream ": ")
                      (depict-type-expr markup-stream world (third tag-spec))
                      (depict markup-stream ")"))
                    (depict markup-stream ":")
                    (depict-break markup-stream 1)
                    (depict-annotated-value-expr markup-stream world value-annotated-expr *primitive-level-stmt*)
                    (when (cdr annotated-cases)
                      (depict markup-stream ";")))))
            annotated-cases)
      (depict-break markup-stream)
      (depict-semantic-keyword markup-stream 'end))))


; (select <tag> <oneof-expr>)
; (& <tag> <tuple-expr>)
(defun depict-select-or-& (markup-stream world level tag annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-unary-suffix*)
    (depict-annotated-value-expr markup-stream world annotated-expr *primitive-level-unary-suffix*)
    (depict markup-stream ".")
    (depict-field-name markup-stream tag)))


; (is <tag> <oneof-expr>)
(defun depict-is (markup-stream world level tag oneof-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-relational*)
    (depict-annotated-value-expr markup-stream world oneof-annotated-expr *primitive-level-unary-suffix*)
    (depict-space markup-stream)
    (depict-semantic-keyword markup-stream 'is)
    (depict-space markup-stream)
    (depict-field-name markup-stream tag)))


;;; Tuples

; (tuple <tuple-type> <field-expr1> ... <field-exprn>)
(defun depict-tuple-form (markup-stream world level type-expr &rest annotated-exprs)
  (declare (ignore level))
  (depict-list markup-stream
               #'(lambda (markup-stream parameter)
                   (depict-annotated-value-expr markup-stream world parameter))
               annotated-exprs
               :indent 4
               :prefix ':tuple-begin
               :prefix-break 0
               :suffix ':tuple-end
               :separator ","
               :break 1
               :empty nil)
  (depict-subscript-type-expr markup-stream world type-expr))


;;; Addresses

; (new <value-expr>)
(defun depict-new (markup-stream world level value-annotated-expr)
  (depict-logical-block (markup-stream 5)
    (depict-semantic-keyword markup-stream 'new)
    (depict-space markup-stream)
    (depict-expr-parentheses (markup-stream level *primitive-level-unparenthesized-new*)
      (depict-annotated-value-expr markup-stream world value-annotated-expr
                                   (if (< level *primitive-level-unparenthesized-new*)
                                     *primitive-level-expr*
                                     *primitive-level-unary-prefix*)))))


; (@ <address-expr>)
(defun depict-@ (markup-stream world level address-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-unary-prefix*)
    (depict-logical-block (markup-stream 2)
      (depict markup-stream "@")
      (depict-annotated-value-expr markup-stream world address-annotated-expr *primitive-level-unary-prefix*))))


; (@= <address-expr> <value-expr>)
(defun depict-@= (markup-stream world level address-annotated-expr value-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-stmt*)
    (depict-logical-block (markup-stream 0)
      (depict markup-stream "@")
      (depict-annotated-value-expr markup-stream world address-annotated-expr *primitive-level-unary-prefix*)
      (depict markup-stream " :=")
      (depict-logical-block (markup-stream 6)
        (depict-break markup-stream 1)
        (depict-annotated-value-expr markup-stream world value-annotated-expr *primitive-level-stmt*)))))


; (address-equal <address-expr1> <address-expr2>)
(defun depict-address-equal (markup-stream world level address1-annotated-expr address2-annotated-expr)
  (depict-expr-parentheses (markup-stream level *primitive-level-relational*)
    (depict-logical-block (markup-stream 0)
      (depict-annotated-value-expr markup-stream world address1-annotated-expr *primitive-level-additive*)
      (depict markup-stream " " :identical-10)
      (depict-break markup-stream 1)
      (depict-annotated-value-expr markup-stream world address2-annotated-expr *primitive-level-additive*))))


;;; Macros

(defun depict-let-binding (markup-stream world var type-expr value-annotated-expr)
  (depict-logical-block (markup-stream 4)
    (depict-local-variable markup-stream var)
    (depict markup-stream ": ")
    (depict-type-expr markup-stream world type-expr)
    (depict-break markup-stream 1)
    (depict-logical-block (markup-stream 3)
      (depict markup-stream "= ")
      (depict-annotated-value-expr markup-stream world value-annotated-expr *primitive-level-stmt*))))


(defun depict-let-body (markup-stream world body-annotated-expr)
  (depict-break markup-stream)
  (depict-semantic-keyword markup-stream 'in)
  (depict-space markup-stream)
  (depict-logical-block (markup-stream (if (or (macro-annotated-expr? 'let body-annotated-expr)
                                               (macro-annotated-expr? 'letexc body-annotated-expr))
                                         nil
                                         4))
    (depict-annotated-value-expr markup-stream world body-annotated-expr *primitive-level-stmt*)))


; (let ((<var1> <type1> <expr1> [:unused]) ... (<varn> <typen> <exprn> [:unused])) <body>)  ==>
; ((lambda ((<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <body>) <expr1> ... <exprn>)
(defun depict-let (markup-stream world level annotated-expansion)
  (assert-true (eq (first annotated-expansion) 'expr-annotation:call))
  (let ((lambda-annotated-expr (second annotated-expansion))
        (arg-annotated-exprs (cddr annotated-expansion)))
    (assert-true (special-form-annotated-expr? 'lambda lambda-annotated-expr))
    (let ((arg-binding-exprs (third lambda-annotated-expr))
          (body-annotated-expr (fourth lambda-annotated-expr)))
      (depict-statement (markup-stream 'let)
        (depict-list markup-stream
                     #'(lambda (markup-stream arg-binding)
                         (depict-let-binding markup-stream world (first arg-binding) (second arg-binding) (pop arg-annotated-exprs)))
                     arg-binding-exprs
                     :indent 4
                     :separator ";"
                     :break t
                     :empty nil)
        (depict-let-body markup-stream world body-annotated-expr)))))


; (letexc (<var> <type> <expr> [:unused]) <body>)  ==>
; (case <expr>
;   ((abrupt x exception) (typed-oneof <body-type> abrupt x))
;   ((normal <var> <type> [:unused]) <body>)))
(defun depict-letexc (markup-stream world level annotated-expansion)
  (assert-true (special-form-annotated-expr? 'case annotated-expansion))
  (let* ((expr-annotated-expr (third annotated-expansion))
         (abrupt-binding (fourth annotated-expansion))
         (abrupt-tag-spec (first abrupt-binding))
         (normal-binding (fifth annotated-expansion))
         (normal-tag-spec (first normal-binding)))
    (assert-true (equal (first abrupt-tag-spec) '(abrupt)))
    (assert-true (equal (first normal-tag-spec) '(normal)))
    (let* ((var (second normal-tag-spec))
           (type-expr (third normal-tag-spec))
           (body-annotated-expr (second normal-binding)))
      (depict-statement (markup-stream 'letexc)
        (depict-logical-block (markup-stream 9)
          (depict-let-binding markup-stream world var type-expr expr-annotated-expr))
        (depict-let-body markup-stream world body-annotated-expr)))))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING COMMANDS


(defmacro depict-semantics ((markup-stream depict-env &optional (paragraph-style ':semantics)) &body body)
  `(progn
     (depict-mode ,markup-stream ,depict-env :semantics)
     (depict-paragraph (,markup-stream ,paragraph-style)
       ,@body)))


; (%section "section-name")
(defun depict-%section (markup-stream world depict-env section-name)
  (declare (ignore world))
  (assert-type section-name string)
  (depict-mode markup-stream depict-env nil)
  (depict-paragraph (markup-stream :section-heading)
    (depict markup-stream section-name)))


; (%subsection "subsection-name")
(defun depict-%subsection (markup-stream world depict-env section-name)
  (declare (ignore world))
  (assert-type section-name string)
  (depict-mode markup-stream depict-env nil)
  (depict-paragraph (markup-stream :subsection-heading)
    (depict markup-stream section-name)))


; (grammar-argument <argument> <attribute> <attribute> ... <attribute>)
(defun depict-grammar-argument (markup-stream world depict-env argument &rest attributes)
  (declare (ignore world))
  (let ((seen-grammar-arguments (depict-env-seen-grammar-arguments depict-env))
        (abbreviated-argument (symbol-abbreviation argument)))
    (unless (gethash abbreviated-argument seen-grammar-arguments)
      (depict-mode markup-stream depict-env :syntax)
      (depict-paragraph (markup-stream :grammar-argument)
        (depict-nonterminal-argument markup-stream argument)
        (depict markup-stream " " :member-10 " ")
        (depict-list markup-stream
                     #'(lambda (markup-stream attribute)
                         (depict-nonterminal-attribute markup-stream attribute))
                     attributes
                     :prefix "{"
                     :suffix "}"
                     :separator ", "))
      (setf (gethash abbreviated-argument seen-grammar-arguments) t))))


; (%rule <general-nonterminal-source>)
(defun depict-%rule (markup-stream world depict-env general-nonterminal-source)
  (declare (ignore world))
  (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
         (grammar (grammar-info-grammar grammar-info))
         (general-nonterminal (grammar-parametrization-intern grammar general-nonterminal-source))
         (seen-nonterminals (depict-env-seen-nonterminals depict-env)))
    (when (grammar-info-charclass grammar-info general-nonterminal)
      (error "Shouldn't use %rule on a lexer charclass nonterminal ~S" general-nonterminal))
    (labels
      ((seen-nonterminal? (nonterminal)
         (gethash nonterminal seen-nonterminals)))
      (unless (every #'seen-nonterminal? (general-grammar-symbol-instances grammar general-nonterminal))
        (depict-mode markup-stream depict-env :syntax)
        (dolist (general-rule (grammar-general-rules grammar general-nonterminal))
          (let ((rule-lhs-nonterminals (general-grammar-symbol-instances grammar (general-rule-lhs general-rule))))
            (unless (every #'seen-nonterminal? rule-lhs-nonterminals)
              (when (some #'seen-nonterminal? rule-lhs-nonterminals)
                (warn "General rule for ~S listed before specific ones; use %rule to disambiguate" general-nonterminal))
              (depict-general-rule markup-stream general-rule)
              (dolist (nonterminal rule-lhs-nonterminals)
                (setf (gethash nonterminal seen-nonterminals) t)))))))))
;******** May still have a problem when a specific rule precedes a general one.


; (%charclass <nonterminal>)
(defun depict-%charclass (markup-stream world depict-env nonterminal)
  (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
         (charclass (grammar-info-charclass grammar-info nonterminal)))
    (unless charclass
      (error "%charclass with a non-charclass ~S" nonterminal))
    (if (gethash nonterminal (depict-env-seen-nonterminals depict-env))
      (warn "Duplicate charclass ~S" nonterminal)
      (progn
        (depict-mode markup-stream depict-env :syntax)
        (depict-charclass markup-stream charclass)
        (dolist (action-cons (charclass-actions charclass))
          (depict-charclass-action world depict-env (cdr action-cons) nonterminal))
        (setf (gethash nonterminal (depict-env-seen-nonterminals depict-env)) t)))))


; (%print-actions)
(defun depict-%print-actions (markup-stream world depict-env)
  (declare (ignore world))
  (dolist (pending-action (nreverse (depict-env-pending-actions-reverse depict-env)))
    (funcall pending-action markup-stream depict-env))
  (setf (depict-env-pending-actions-reverse depict-env) nil))


; (deftype <name> <type>)
(defun depict-deftype (markup-stream world depict-env name type-expr)
  (depict-semantics (markup-stream depict-env)
    (depict-logical-block (markup-stream 2)
      (depict-semantic-keyword markup-stream 'type)
      (depict-space markup-stream)
      (depict-type-name markup-stream name)
      (depict-break markup-stream 1)
      (depict-logical-block (markup-stream 3)
        (depict markup-stream "= ")
        (depict-type-expr markup-stream world type-expr)))))


; (define <name> <type> <value> <destructured>)
; <destructured> is a flag that is true if this define was originally in the form
;   (define (<name> (<arg1> <type1>) ... (<argn> <typen>)) <result-type> <value>)
; and converted into
;   (define <name> (-> (<type1> ... <typen>) <result-type>)
;      (lambda ((<arg1> <type1>) ... (<argn> <typen>)) <value>)
;      t)
(defun depict-define (markup-stream world depict-env name type-expr value-expr destructured)
  (depict-semantics (markup-stream depict-env)
    (depict-logical-block (markup-stream 2)
      (depict-global-variable markup-stream name)
      (flet
        ((depict-type-and-value (markup-stream type-expr annotated-value-expr)
           (depict-logical-block (markup-stream 0)
             (depict-break markup-stream 1)
             (depict markup-stream ": ")
             (depict-type-expr markup-stream world type-expr))
           (depict-break markup-stream 1)
           (depict-logical-block (markup-stream 3)
             (depict markup-stream "= ")
             (depict-annotated-value-expr markup-stream world annotated-value-expr *primitive-level-max*))))
        
        (let ((annotated-value-expr (nth-value 2 (scan-value world *null-type-env* value-expr))))
          (if destructured
            (progn
              (assert-true (eq (first type-expr) '->))
              (assert-true (special-form-annotated-expr? 'lambda annotated-value-expr))
              (depict-lambda-bindings markup-stream world (third annotated-value-expr))
              (depict-type-and-value markup-stream (third type-expr) (fourth annotated-value-expr)))
            (depict-type-and-value markup-stream type-expr annotated-value-expr)))))))


; (set-grammar <name>)
(defun depict-set-grammar (markup-stream world depict-env name)
  (depict-clear-grammar markup-stream world depict-env)
  (let ((grammar-info (world-grammar-info world name)))
    (unless grammar-info
      (error "Unknown grammar ~A" name))
    (setf (depict-env-grammar-info depict-env) grammar-info)
    (setf (depict-env-seen-nonterminals depict-env) (make-hash-table :test #'eq))
    (setf (depict-env-seen-grammar-arguments depict-env) (make-hash-table :test #'eq))))


; (clear-grammar)
(defun depict-clear-grammar (markup-stream world depict-env)
  (depict-%print-actions markup-stream world depict-env)
  (depict-mode markup-stream depict-env nil)
  (let ((grammar-info (depict-env-grammar-info depict-env)))
    (when grammar-info
      (let ((seen-nonterminals (depict-env-seen-nonterminals depict-env))
            (missed-nonterminals nil))
        (dolist (nonterminal (grammar-nonterminals-list (grammar-info-grammar grammar-info)))
          (unless (or (gethash nonterminal seen-nonterminals)
                      (eq nonterminal *start-nonterminal*))
            (push nonterminal missed-nonterminals)))
        (when missed-nonterminals
          (warn "Nonterminals not printed: ~S" missed-nonterminals)))
      (setf (depict-env-grammar-info depict-env) nil)
      (setf (depict-env-seen-nonterminals depict-env) nil)
      (setf (depict-env-seen-grammar-arguments depict-env) nil))))


(defmacro depict-delayed-action ((markup-stream depict-env) &body depictor)
  `(push #'(lambda (,markup-stream ,depict-env) ,@depictor)
         (depict-env-pending-actions-reverse ,depict-env)))


(defun depict-declare-action-contents (markup-stream world action-name general-grammar-symbol type-expr)
  (depict-semantic-keyword markup-stream 'action)
  (depict-space markup-stream)
  (depict-action-name markup-stream action-name)
  (depict markup-stream :action-begin)
  (depict-general-grammar-symbol markup-stream general-grammar-symbol)
  (depict markup-stream :action-end)
  (depict-break markup-stream 1)
  (depict-logical-block (markup-stream 2)
    (depict markup-stream ": ")
    (depict-type-expr markup-stream world type-expr)))


; (declare-action <action-name> <general-grammar-symbol> <type>)
(defun depict-declare-action (markup-stream world depict-env action-name general-grammar-symbol-source type-expr)
  (declare (ignore markup-stream))
  (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
         (general-grammar-symbol (grammar-parametrization-intern (grammar-info-grammar grammar-info) general-grammar-symbol-source)))
    (unless (grammar-info-charclass-or-partition grammar-info general-grammar-symbol)
      (depict-delayed-action (markup-stream depict-env)
        (depict-semantics (markup-stream depict-env)
          (depict-logical-block (markup-stream 4)
            (depict-declare-action-contents markup-stream world action-name general-grammar-symbol type-expr)))))))


; Declare and define the lexer-action on the charclass given by nonterminal.
(defun depict-charclass-action (world depict-env lexer-action nonterminal)
  (depict-delayed-action (markup-stream depict-env)
    (depict-semantics (markup-stream depict-env)
      (depict-logical-block (markup-stream 4)
        (depict-declare-action-contents markup-stream world (lexer-action-name lexer-action)
                                        nonterminal (lexer-action-type-expr lexer-action))
        (depict-break markup-stream 1)
        (depict-logical-block (markup-stream 3)
          (depict markup-stream "= ")
          (depict-lexer-action markup-stream lexer-action nonterminal))))))


; (action <action-name> <production-name> <body>)
; <destructured> is a flag that is true if this define was originally in the form
;   (action (<action-name> (<arg1> <type1>) ... (<argn> <typen>)) <production-name> <body>)
; and converted into
;   (action <action-name> <production-name> (lambda ((<arg1> <type1>) ... (<argn> <typen>)) <body>) t)
(defun depict-action (markup-stream world depict-env action-name production-name body-expr destructured)
  (declare (ignore markup-stream))
  (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
         (grammar (grammar-info-grammar grammar-info))
         (general-production (grammar-general-production grammar production-name)))
    (unless (grammar-info-charclass grammar-info (general-production-lhs general-production))
      (depict-delayed-action (markup-stream depict-env)
        (depict-semantics (markup-stream depict-env :semantics-next)
          (depict-logical-block (markup-stream 2)
            (let* ((initial-env (general-production-action-env grammar general-production))
                   (body-annotated-expr (nth-value 2 (scan-value world initial-env body-expr)))
                   (action-grammar-symbols (annotated-expr-grammar-symbols body-annotated-expr)))
              (depict-action-name markup-stream action-name)
              (depict markup-stream :action-begin)
              (depict-general-production markup-stream general-production action-grammar-symbols)
              (depict markup-stream :action-end)
              (flet
                ((depict-body (markup-stream body-annotated-expr)
                   (depict-break markup-stream 1)
                   (depict-logical-block (markup-stream 3)
                     (depict markup-stream "= ")
                     (depict-annotated-value-expr markup-stream world body-annotated-expr *primitive-level-stmt*))))
                
                (if destructured
                  (progn
                    (assert-true (special-form-annotated-expr? 'lambda body-annotated-expr))
                    (depict-logical-block (markup-stream 10)
                      (depict-break markup-stream 0)
                      (depict-lambda-bindings markup-stream world (third body-annotated-expr)))
                    (depict-body markup-stream (fourth body-annotated-expr)))
                  (depict-body markup-stream body-annotated-expr))))))))))


; (terminal-action <action-name> <terminal> <lisp-function-name>)
(defun depict-terminal-action (markup-stream world depict-env action-name terminal function-name)
  (declare (ignore markup-stream world depict-env action-name terminal function-name)))
