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
;;; ECMAScript semantic calculus markup emitters
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(defvar *hide-$-nonterminals* t) ; Should rules and actions expanding nonterminals starting with $ be invisible?
(defvar *depict-trivial-functions-as-expressions* nil)

(defvar *styled-text-world*)


(defun hidden-nonterminal? (general-nonterminal)
  (and *hide-$-nonterminals*
       (eql (first-symbol-char (general-grammar-symbol-symbol general-nonterminal)) #\$)))


; Return true if this action call should be replaced by a plain reference to the action's nonterminal.
(defun default-action? (action-name)
  (equal (symbol-name action-name) "$DEFAULT-ACTION"))


;;; ------------------------------------------------------------------------------------------------------
;;; SEMANTIC DEPICTION UTILITIES

(defparameter *semantic-keywords*
  '(not and or xor mod new eltof
    some every satisfies
    such that
    tag tuple record
    proc
    begin end nothing
    if then elsif else
    while do
    invariant
    return
    throw try catch
    case of))

; Emit markup for one of the semantic keywords, as specified by keyword-symbol.
; space can be either nil, :before, or :after to indicate space placement.
(defun depict-semantic-keyword (markup-stream keyword-symbol space)
  (assert-true (and (member keyword-symbol *semantic-keywords*)
                    (member space '(nil :before :after))))
  (when (eq space :before)
    (depict-space markup-stream))
  (depict-char-style (markup-stream :semantic-keyword)
    (depict markup-stream (string-downcase (symbol-name keyword-symbol))))
  (when (eq space :after)
    (depict-space markup-stream)))


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
(defstruct (depict-env (:constructor make-depict-env (visible-semantics heading-offset)))
  (visible-semantics t :type bool :read-only t)                  ;Nil if semantics are not to be depicted
  (heading-offset 0 :type integer :read-only t)                  ;Offset to be added to each heading level when depicting it
  (grammar-info nil :type (or null grammar-info))                ;The current grammar-info or nil if none
  (seen-nonterminals nil :type (or null hash-table))             ;Hash table (nonterminal -> t) of nonterminals already depicted
  (seen-grammar-arguments nil :type (or null hash-table))        ;Hash table (grammar-argument -> t) of grammar-arguments already depicted
  (mode nil :type (member nil :syntax :semantics))               ;Current heading (:syntax or :semantics) or nil if none
  (pending-actions-reverse nil :type list))                      ;Reverse-order list of (action-name . closure) of actions pending for a %print-actions


(defun checked-depict-env-grammar-info (depict-env)
  (or (depict-env-grammar-info depict-env)
      (error "Grammar needed")))


; Set the mode to the given mode without emitting any headings.
; Return true if the contents should be visible, nil if not.
(defun quiet-depict-mode (depict-env mode)
  (unless (member mode '(nil :syntax :semantics))
    (error "Bad mode: ~S" mode))
  (setf (depict-env-mode depict-env) mode)
  (or (depict-env-visible-semantics depict-env)
      (not (eq mode :semantics))))


; Set the mode to the given mode, emitting a heading if necessary.
; Return true if the contents should be visible, nil if not.
(defun depict-mode (markup-stream depict-env mode)
  (unless (eq mode (depict-env-mode depict-env))
    (when (depict-env-visible-semantics depict-env)
      (ecase mode
        (:syntax (depict-paragraph (markup-stream :grammar-header)
                   (depict markup-stream "Syntax")))
        (:semantics (depict-paragraph (markup-stream :grammar-header)
                      (depict markup-stream "Semantics")))
        ((nil)))))
  (quiet-depict-mode depict-env mode))


; Set the mode to :semantics, always emitting a heading with the given group-name string.
; Return true if the contents should be visible, nil if not.
(defun depict-semantic-group (markup-stream depict-env group-name)
  (cond
   ((depict-env-visible-semantics depict-env)
    (depict-paragraph (markup-stream :grammar-header)
      (depict markup-stream group-name))
    (setf (depict-env-mode depict-env) :semantics)
    t)
   (t nil)))


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


; Emit markup paragraphs for a list of commands.
(defun depict-commands (markup-stream world depict-env commands)
  (dolist (command commands)
    (depict-command markup-stream world depict-env command)))


; Emit markup paragraphs for the world's commands.
(defun depict-world-commands (markup-stream world &key (visible-semantics t) (heading-offset 0))
  (let ((depict-env (make-depict-env visible-semantics heading-offset)))
    (depict-commands markup-stream world depict-env (world-commands-source world))
    (depict-clear-grammar markup-stream world depict-env)))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING TAGS


(defparameter *tag-name-special-cases*
  '((:+zero "PlusZero" "+zero")
    (:-zero "MinusZero" (:minus "zero"))
    (:+infinity "PlusInfinity" ("+" :infinity))
    (:-infinity "MinusInfinity" (:minus :infinity))
    (:nan "NaN" "NaN")))


; Return two values:
;   A string to use as the tag's link name;
;   A depict item or list of items forming the tag's name.
(defun tag-link-name-and-name (tag)
  (let ((special-case (assoc (tag-keyword tag) *tag-name-special-cases*)))
    (if special-case
      (values (second special-case) (third special-case))
      (let ((name (symbol-lower-mixed-case-name (tag-name tag))))
        (values name name)))))


; Emit markup for a tag.
; link should be one of:
;   :reference   if this is a reference or external reference of this tag;
;   :definition  if this is a definition of this tag;
;   nil          if this use of the tag should not be cross-referenced.
(defun depict-tag-name (markup-stream tag link)
  (assert-true (tag-keyword tag))
  (when (eq link :reference)
    (setq link (tag-link tag)))
  (multiple-value-bind (link-name name) (tag-link-name-and-name tag)
    (depict-link (markup-stream link "T-" link-name nil)
      (depict-char-style (markup-stream :tag-name)
        (depict-item-or-list markup-stream name)))))


; Emit markup for a tuple or record type's label, which must be a symbol.
; link should be one of:
;   :reference   if this is a reference or external reference to this label;
;   nil          if this use of the label should not be cross-referenced.
(defun depict-label-name (markup-stream type label link)
  (unless (type-has-field type label)
    (error "Type ~A doesn't have label ~A" type label))
  (let ((type-name (type-name type)))
    (unless type-name
      ;(warn "Accessing field ~A of anonymous type ~S" label type)
      (setq link nil))
    (depict-link (markup-stream link "D-" (symbol-upper-mixed-case-name type-name) nil)
      (depict-char-style (markup-stream :field-name)
        (depict markup-stream (symbol-lower-mixed-case-name label))))))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING TYPES

;;; The level argument indicates what kinds of component types may be represented without being placed
;;; in parentheses.

(defparameter *type-level* (make-partial-order))
(def-partial-order-element *type-level* %%primary%%)                              ;id, tuple, (type)
(def-partial-order-element *type-level* %%suffix%% %%primary%%)                   ;type[], type{}
(def-partial-order-element *type-level* %%function%% %%suffix%%)                  ;type x type -> type
(def-partial-order-element *type-level* %%type%% %%function%%)                    ;type U type


; Emit markup for the name of a type, which must be a symbol.
; link should be one of:
;   :reference   if this is a reference of this type name;
;   :external    if this is an external reference of this type name;
;   :definition  if this is a definition of this type name;
;   nil          if this use of the type name should not be cross-referenced.
(defun depict-type-name (markup-stream type-name link)
  (let ((name (symbol-upper-mixed-case-name type-name)))
    (depict-link (markup-stream link "D-" name nil)
      (depict-char-style (markup-stream :domain-name)
        (depict markup-stream name)))))


; If level < threshold, depict an opening parenthesis, evaluate body, and depict a closing
; parentheses.  Otherwise, just evaluate body.
; Return the result value of body.
(defmacro depict-type-parentheses ((markup-stream level threshold) &body body)
  `(depict-optional-parentheses (,markup-stream (partial-order-< ,level ,threshold))
     ,@body))


; Emit markup for the given type expression.  level is non-nil if this is a recursive
; call to depict-type-expr; in this case level indicates the binding level imposed by the
; enclosing type expression.
(defun depict-type-expr (markup-stream world type-expr &optional level)
  (cond
   ((identifier? type-expr)
    (let ((type-name (world-intern world type-expr)))
      (depict-type-name markup-stream type-expr (if (symbol-type-user-defined type-name) :reference :external))))
   ((consp type-expr)
    (let ((depictor (get (world-intern world (first type-expr)) :depict-type-constructor)))
      (apply depictor markup-stream world (or level %%type%%) (rest type-expr))))
   (t (error "Bad type expression: ~S" type-expr))))


; (-> (<arg-type1> ... <arg-typen>) <result-type>)
;   "<arg-type1> x ... x <arg-typen> -> <result-type>"
(defun depict--> (markup-stream world level arg-type-exprs result-type-expr)
  (depict-type-parentheses (markup-stream level %%function%%)
    (depict-list markup-stream
                 #'(lambda (markup-stream arg-type-expr)
                     (depict-type-expr markup-stream world arg-type-expr %%suffix%%))
                 arg-type-exprs
                 :separator '(" " :cartesian-product-10 " ")
                 :empty "()")
    (depict markup-stream " " :function-arrow-10 " ")
    (if (eq result-type-expr 'void)
      (depict markup-stream "()")
      (depict-type-expr markup-stream world result-type-expr %%suffix%%))))


; (vector <element-type>)
;   "<element-type>[]"
(defun depict-vector (markup-stream world level element-type-expr)
  (depict-type-parentheses (markup-stream level %%suffix%%)
    (depict-type-expr markup-stream world element-type-expr %%suffix%%)
    (depict markup-stream "[]")))


; (range-set <element-type>)
;   "<element-type>{}"
(defun depict-set (markup-stream world level element-type-expr)
  (depict-type-parentheses (markup-stream level %%suffix%%)
    (depict-type-expr markup-stream world element-type-expr %%suffix%%)
    (depict markup-stream "{}")))


; (tag <tag> ... <tag>)
;   "{<tag> *, ..., <tag> *}"
(defun depict-tag-type (markup-stream world level &rest tag-names)
  (declare (ignore level))
  (depict-list
   markup-stream
   #'(lambda (markup-stream tag-name)
       (depict-tag-name markup-stream (scan-tag world tag-name) :reference))
   tag-names
   :indent 1
   :prefix "{"
   :suffix "}"
   :separator ","
   :break 1))


; (union <type1> ... <typen>)
;   "<type1> U ... U <typen>"
;   "{}" if no types are given
(defun depict-union (markup-stream world level &rest type-exprs)
  (cond
   ((endp type-exprs) (depict markup-stream "{}"))
   ((endp (cdr type-exprs)) (depict-type-expr markup-stream world (first type-exprs) level))
   (t (depict-type-parentheses (markup-stream level %%type%%)
        (depict-list markup-stream
                     #'(lambda (markup-stream type-expr)
                         (depict-type-expr markup-stream world type-expr %%function%%))
                     type-exprs
                     :indent 0
                     :separator '(" " :union-10)
                     :break 1)))))


; (writable-cell <element-type>)
;   "<element-type>"
(defun depict-writable-cell (markup-stream world level element-type-expr)
  (depict-type-expr markup-stream world element-type-expr level))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING EXPRESSIONS


;;; The level argument indicates what kinds of subexpressions may be represented without being placed
;;; in parentheses (or on a separate line for the case of function and if/then/else).


; If primitive-level is not a superset of threshold, depict an opening parenthesis,
; evaluate body, and depict a closing parentheses.  Otherwise, just evaluate body.
; Return the result value of body.
(defmacro depict-expr-parentheses ((markup-stream primitive-level threshold) &body body)
  `(depict-optional-parentheses (,markup-stream (partial-order-< ,primitive-level ,threshold))
     ,@body))


; Emit markup for the name of a global variable, which must be a symbol.
; link should be one of:
;   :reference   if this is a reference of this global variable;
;   :external    if this is an external reference of this global variable;
;   :definition  if this is a definition of this global variable;
;   nil          if this use of the global variable should not be cross-referenced.
(defun depict-global-variable (markup-stream global-name link)
  (let ((name (symbol-lower-mixed-case-name global-name)))
    (depict-link (markup-stream link "V-" name nil)
      (depict-char-style (markup-stream :global-variable)
        (depict markup-stream name)))))


; Emit markup for the name of a local variable, which must be a symbol.
(defun depict-local-variable (markup-stream name)
  (depict-char-style (markup-stream :variable)
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
    (depict markup-stream
            (if (zerop constant)
              (if (minusp (float64-sign constant)) "-0.0" "+0.0")
              (progn
                (when (minusp constant)
                  (depict markup-stream :minus)
                  (setq constant (- constant)))
                (format nil (if (= constant (floor constant 1)) "~,1F" "~F") constant)))))
   ((characterp constant)
    (depict markup-stream :left-single-quote)
    (depict-char-style (markup-stream :character-literal)
      (depict-character markup-stream constant nil))
    (depict markup-stream :right-single-quote))
   ((stringp constant)
    (depict-string markup-stream constant))
   (t (error "Bad constant ~S" constant))))


; Emit markup for the primitive when it is not called in a function call.
(defun depict-primitive (markup-stream primitive)
  (unless (eq (primitive-appearance primitive) :global)
    (error "Can't depict primitive ~S outside a call" primitive))
  (let ((markup (primitive-markup1 primitive))
        (external-name (primitive-markup2 primitive)))
    (if external-name
      (depict-link (markup-stream :external "V-" external-name nil)
        (depict-item-or-group-list markup-stream markup))
      (depict-item-or-group-list markup-stream markup))))


; Emit markup for the parameters to a function call.
(defun depict-call-parameters (markup-stream world annotated-parameters)
  (depict-list markup-stream
               #'(lambda (markup-stream parameter)
                   (depict-expression markup-stream world parameter %expr%))
               annotated-parameters
               :indent 4
               :prefix "("
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
             (depict-expression markup-stream world (first annotated-arg-exprs) (primitive-level1 primitive))
             (let ((spaces (primitive-markup2 primitive)))
               (when spaces
                 (depict-space markup-stream))
               (depict-item-or-group-list markup-stream (primitive-markup1 primitive))
               (when spaces
                 (depict-break markup-stream 1)))
             (depict-expression markup-stream world (second annotated-arg-exprs) (primitive-level2 primitive))))
          (:unary
           (assert-true (= (length annotated-arg-exprs) 1))
           (depict-item-or-group-list markup-stream (primitive-markup1 primitive))
           (depict-expression markup-stream world (first annotated-arg-exprs) (primitive-level1 primitive))
           (depict-item-or-group-list markup-stream (primitive-markup2 primitive)))
          (:phantom
           (assert-true (= (length annotated-arg-exprs) 1))
           (depict-expression markup-stream world (first annotated-arg-exprs) level)))))
    (depict-expr-parentheses (markup-stream level %suffix%)
      (depict-expression markup-stream world annotated-function-expr %suffix%)
      (depict-call-parameters markup-stream world annotated-arg-exprs))))


; Emit markup for the reference to the action on the given general grammar symbol.
(defun depict-action-reference (markup-stream action-name general-grammar-symbol &optional index)
  (let ((action-default (default-action? action-name)))
    (unless action-default
      (depict-action-name markup-stream action-name)
      (depict markup-stream :action-begin))
    (depict-general-grammar-symbol markup-stream general-grammar-symbol :reference index)
    (unless action-default
      (depict markup-stream :action-end))))


; Emit markup for the given annotated value expression. level indicates the binding level imposed
; by the enclosing expression.
(defun depict-expression (markup-stream world annotated-expr level)
  (let ((annotation (first annotated-expr))
        (args (rest annotated-expr)))
    (ecase annotation
      (expr-annotation:constant (depict-constant markup-stream (first args)))
      (expr-annotation:primitive (depict-primitive markup-stream (symbol-primitive (first args))))
      (expr-annotation:tag (depict-tag-name markup-stream (first args) :reference))
      (expr-annotation:local (depict-local-variable markup-stream (first args)))
      (expr-annotation:global (depict-global-variable markup-stream (first args) :reference))
      (expr-annotation:call (apply #'depict-call markup-stream world level args))
      (expr-annotation:action (apply #'depict-action-reference markup-stream args))
      (expr-annotation:special-form
       (apply (get (first args) :depict-special-form) markup-stream world level (rest args))))))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING SPECIAL FORMS


; (bottom)
(defun depict-bottom (markup-stream world level)
  (declare (ignore world level))
  (depict markup-stream :bottom-10))


; (todo)
(defun depict-todo (markup-stream world level)
  (declare (ignore world level))
  (depict markup-stream "????"))


; (hex <integer> [<length>])
(defun depict-hex (markup-stream world level n length)
  (if (minusp n)
    (progn
      (depict markup-stream "-")
      (depict-hex markup-stream world level (- n) length))
    (depict markup-stream (format nil "0x~V,'0X" length n))))


; (expt <base> <exponent>)
(defun depict-expt (markup-stream world level base-annotated-expr exponent-annotated-expr)
  (depict-expr-parentheses (markup-stream level %prefix%)
    (depict-expression markup-stream world base-annotated-expr %primary%)
    (depict-char-style (markup-stream :superscript)
      (depict-expression markup-stream world exponent-annotated-expr %term%))))


; (= <expr1> <expr2> [<type>])
; (/= <expr1> <expr2> [<type>])
; (< <expr1> <expr2> [<type>])
; (> <expr1> <expr2> [<type>])
; (<= <expr1> <expr2> [<type>])
; (>= <expr1> <expr2> [<type>])
(defun depict-comparison (markup-stream world level order annotated-expr1 annotated-expr2)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-logical-block (markup-stream 0)
      (depict-expression markup-stream world annotated-expr1 %term%)
      (depict-space markup-stream)
      (depict markup-stream order)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world annotated-expr2 %term%))))


; (cascade <type> <expr1> <order1> <expr2> <order2> ... <ordern-1> <exprn>)
(defun depict-cascade (markup-stream world level annotated-expr1 &rest orders-and-exprs)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-logical-block (markup-stream 0)
      (depict-expression markup-stream world annotated-expr1 %term%)
      (do ()
          ((endp orders-and-exprs))
        (depict-space markup-stream)
        (depict markup-stream (pop orders-and-exprs))
        (depict-break markup-stream 1)
        (depict-expression markup-stream world (pop orders-and-exprs) %term%)))))


; (and <expr> ... <expr>)
; (or <expr> ... <expr>)
; (xor <expr> ... <expr>)
(defun depict-and-or-xor (markup-stream world level op annotated-expr &rest annotated-exprs)
  (if annotated-exprs
    (depict-expr-parentheses (markup-stream level %logical%)
      (depict-logical-block (markup-stream 0)
        (depict-expression markup-stream world annotated-expr %relational%)
        (dolist (annotated-expr annotated-exprs)
          (depict-semantic-keyword markup-stream op :before)
          (depict-break markup-stream 1)
          (depict-expression markup-stream world annotated-expr %relational%))))
    (depict-expression markup-stream world annotated-expr level)))


; (lambda ((<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <result-type> . <statements>)
(defun depict-lambda (markup-stream world level arg-binding-exprs result-type-expr &rest body-annotated-stmts)
  (declare (ignore markup-stream world level arg-binding-exprs result-type-expr body-annotated-stmts))
  (error "Depiction of raw lambdas not supported"))
#|
(defun depict-lambda (markup-stream world level arg-binding-exprs result-type-expr &rest body-annotated-stmts)
  (depict-expr-parentheses (markup-stream level %expr%)
    (depict-logical-block (markup-stream 0)
      (depict-semantic-keyword markup-stream 'proc nil)
      (depict-function-signature markup-stream world arg-binding-exprs result-type-expr t)
      (depict-function-body markup-stream world nil :statement body-annotated-stmts))))
|#


; (if <condition-expr> <true-expr> <false-expr>)
(defun depict-if-expr (markup-stream world level condition-annotated-expr true-annotated-expr false-annotated-expr)
  (depict-expr-parentheses (markup-stream level %expr%)
    (depict-expression markup-stream world condition-annotated-expr %logical%)
    (depict markup-stream " ?")
    (depict-logical-block (markup-stream 4)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world true-annotated-expr %logical%)
      (depict markup-stream " :")
      (depict-break markup-stream 1)
      (depict-expression markup-stream world false-annotated-expr %logical%))))


;;; Vectors

; (vector <element-expr> <element-expr> ... <element-expr>)
; (vector-of <element-type> <element-expr> ... <element-expr>)
(defun depict-vector-expr (markup-stream world level &rest element-annotated-exprs)
  (declare (ignore level))
  (if element-annotated-exprs
    (depict-list markup-stream
                 #'(lambda (markup-stream element-annotated-expr)
                     (depict-expression markup-stream world element-annotated-expr %expr%))
                 element-annotated-exprs
                 :indent 1
                 :prefix :vector-begin
                 :suffix :vector-end
                 :separator ","
                 :break 1)
    (depict markup-stream :empty-vector)))


#|
(defun depict-subscript-type-expr (markup-stream world type-expr)
  (depict-char-style (markup-stream 'sub)
    (depict-type-expr markup-stream world type-expr)))
|#


#|
(defun depict-special-function (markup-stream world name-str &rest arg-annotated-exprs)
  (depict-link (markup-stream :external "V-" name-str nil)
    (depict-char-style (markup-stream :global-variable)
      (depict markup-stream name-str)))
  (depict-call-parameters markup-stream world arg-annotated-exprs))
|#


; (nth <vector-expr> <n-expr>)
(defun depict-nth (markup-stream world level vector-annotated-expr n-annotated-expr)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-expression markup-stream world vector-annotated-expr %suffix%)
    (depict markup-stream "[")
    (depict-expression markup-stream world n-annotated-expr %expr%)
    (depict markup-stream "]")))


; (subseq <vector-expr> <low-expr> [<high-expr>])
(defun depict-subseq (markup-stream world level vector-annotated-expr low-annotated-expr high-annotated-expr)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-expression markup-stream world vector-annotated-expr %suffix%)
    (depict-logical-block (markup-stream 4)
      (depict markup-stream "[")
      (depict-expression markup-stream world low-annotated-expr %term%)
      (depict markup-stream " ...")
      (when high-annotated-expr
        (depict-break markup-stream 1)
        (depict-expression markup-stream world high-annotated-expr %term%))
      (depict markup-stream "]"))))


; (append <vector-expr> <vector-expr>)
(defun depict-append (markup-stream world level vector1-annotated-expr vector2-annotated-expr)
  (depict-expr-parentheses (markup-stream level %term%)
    (depict-logical-block (markup-stream 0)
      (depict-expression markup-stream world vector1-annotated-expr %term%)
      (depict markup-stream " " :vector-append)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world vector2-annotated-expr %term%))))


; (set-nth <vector-expr> <n-expr> <value-expr>)
(defun depict-set-nth (markup-stream world level vector-annotated-expr n-annotated-expr value-annotated-expr)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-expression markup-stream world vector-annotated-expr %suffix%)
    (depict-logical-block (markup-stream 4)
      (depict markup-stream "[")
      (depict-expression markup-stream world n-annotated-expr %term%)
      (depict markup-stream " \\")
      (depict-break markup-stream 1)
      (depict-expression markup-stream world value-annotated-expr %term%)
      (depict markup-stream "]"))))


;;; Sets

; (list-set <element-expr> ... <element-expr>)
; (list-set-of <element-type> <element-expr> ... <element-expr>)
(defun depict-list-set-expr (markup-stream world level &rest element-annotated-exprs)
  (declare (ignore level))
  (depict-list markup-stream
               #'(lambda (markup-stream element-annotated-expr)
                   (depict-expression markup-stream world element-annotated-expr %expr%))
               element-annotated-exprs
               :indent 1
               :prefix "{"
               :suffix "}"
               :separator ","
               :break 1
               :empty nil))


; (range-set-of-ranges <element-type> <low-expr> <high-expr> ... <low-expr> <high-expr>)
(defun depict-range-set-of-ranges (markup-stream world level &rest element-annotated-exprs)
  (declare (ignore level))
  (labels
    ((combine-exprs (element-annotated-exprs)
       (if (endp element-annotated-exprs)
         nil
         (acons (first element-annotated-exprs) (second element-annotated-exprs)
                (combine-exprs (cddr element-annotated-exprs))))))
    (depict-list markup-stream
                 #'(lambda (markup-stream element-annotated-expr-range)
                     (let ((element-annotated-expr1 (car element-annotated-expr-range))
                           (element-annotated-expr2 (cdr element-annotated-expr-range)))
                       (depict-expression markup-stream world element-annotated-expr1 %term%)
                       (when element-annotated-expr2
                         (depict markup-stream " ...")
                         (depict-break markup-stream 1)
                         (depict-expression markup-stream world element-annotated-expr2 %term%))))
                 (combine-exprs element-annotated-exprs)
                 :indent 1
                 :prefix "{"
                 :suffix "}"
                 :separator ","
                 :break 1
                 :empty nil)))


; (set* <set-expr> <set-expr>)
(defun depict-set* (markup-stream world level set1-annotated-expr set2-annotated-expr)
  (depict-expr-parentheses (markup-stream level %factor%)
    (depict-logical-block (markup-stream 0)
      (depict-expression markup-stream world set1-annotated-expr %factor%)
      (depict markup-stream " " :intersection-10)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world set2-annotated-expr %factor%))))


; (set+ <set-expr> <set-expr>)
(defun depict-set+ (markup-stream world level set1-annotated-expr set2-annotated-expr)
  (depict-expr-parentheses (markup-stream level %term%)
    (depict-logical-block (markup-stream 0)
      (depict-expression markup-stream world set1-annotated-expr %term%)
      (depict markup-stream " " :union-10)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world set2-annotated-expr %term%))))


; (set- <set-expr> <set-expr>)
(defun depict-set- (markup-stream world level set1-annotated-expr set2-annotated-expr)
  (depict-expr-parentheses (markup-stream level %term%)
    (depict-logical-block (markup-stream 0)
      (depict-expression markup-stream world set1-annotated-expr %term%)
      (depict markup-stream " " :minus)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world set2-annotated-expr %factor%))))


; (set-in <elt-expr> <set-expr>)
; (set-not-in <elt-expr> <set-expr>)
(defun depict-set-in (markup-stream world level op elt-annotated-expr set-annotated-expr)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-logical-block (markup-stream 0)
      (depict-expression markup-stream world elt-annotated-expr %term%)
      (depict markup-stream " " op)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world set-annotated-expr %term%))))


; (elt-of <elt-expr>)
(defun depict-elt-of (markup-stream world level set-annotated-expr)
  (depict-expr-parentheses (markup-stream level %min-max%)
    (depict-semantic-keyword markup-stream 'eltof :after)
    (depict-expression markup-stream world set-annotated-expr %prefix%)))


;;; Vectors or Sets

(defun depict-empty-set-or-vector (markup-stream kind)
  (ecase kind
    ((:string :vector) (depict markup-stream :empty-vector))
    ((:list-set :range-set) (depict markup-stream "{}"))))


; (empty <vector-or-set-expr>)
(defun depict-empty (markup-stream world level kind vector-annotated-expr)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-expression markup-stream world vector-annotated-expr %term%)
    (depict markup-stream " = ")
    (depict-empty-set-or-vector markup-stream kind)))


; (nonempty <vector-or-set-expr>)
(defun depict-nonempty (markup-stream world level kind vector-annotated-expr)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-expression markup-stream world vector-annotated-expr %term%)
    (depict markup-stream " " :not-equal " ")
    (depict-empty-set-or-vector markup-stream kind)))


; (length <vector-or-set-expr>)
(defun depict-length (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict markup-stream "|")
  (depict-expression markup-stream world vector-annotated-expr %expr%)
  (depict markup-stream "|"))


; (some <vector-or-set-expr> <var> <condition-expr>)
; (every <vector-or-set-expr> <var> <condition-expr>)
(defun depict-some (markup-stream world level keyword collection-annotated-expr var condition-annotated-expr)
  (depict-expr-parentheses (markup-stream level %expr%)
    (depict-logical-block (markup-stream 2)
      (depict-semantic-keyword markup-stream keyword :after)
      (depict-local-variable markup-stream var)
      (depict markup-stream " " :member-10 " ")
      (depict-expression markup-stream world collection-annotated-expr %term%)
      (depict-semantic-keyword markup-stream 'satisfies :before)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world condition-annotated-expr %logical%))))


; (map <vector-or-set-expr> <var> <value-expr> [<condition-expr>])
(defun depict-map (markup-stream world level collection-kind collection-annotated-expr var value-annotated-expr &optional condition-annotated-expr)
  (declare (ignore level))
  (multiple-value-bind (open bar close)
                       (ecase collection-kind
                         ((:string :vector) (values :vector-begin :vector-construct :vector-end))
                         ((:list-set :range-set) (values "{" "|" "}")))
    (depict-logical-block (markup-stream 2)
      (depict markup-stream open)
      (depict-expression markup-stream world value-annotated-expr %expr%)
      (depict markup-stream " " bar)
      (depict-break markup-stream 1)
      (depict markup-stream :for-all-10)
      (depict-local-variable markup-stream var)
      (depict markup-stream " " :member-10 " ")
      (depict-expression markup-stream world collection-annotated-expr %term%)
      (when condition-annotated-expr
        (depict-semantic-keyword markup-stream 'such :before)
        (depict-semantic-keyword markup-stream 'that :before)
        (depict-break markup-stream 1)
        (depict-expression markup-stream world condition-annotated-expr %logical%))
      (depict markup-stream close))))


;;; Tuples and Records

(defparameter *depict-tag-labels* nil)

; (new <type> <field-expr1> ... <field-exprn>)
(defun depict-new (markup-stream world level type type-name &rest annotated-exprs)
  (let* ((tag (type-tag type))
         (mutable (tag-mutable tag)))
    (flet
      ((depict-tag-and-args (markup-stream)
         (let ((fields (tag-fields tag)))
           (assert-true (= (length fields) (length annotated-exprs)))
           (depict-type-name markup-stream type-name :reference)
           (if (tag-keyword tag)
             (assert-true (null annotated-exprs))
             (depict-list markup-stream
                          #'(lambda (markup-stream parameter)
                              (let ((field (pop fields)))
                                (if (and mutable *depict-tag-labels*)
                                  (depict-logical-block (markup-stream 4)
                                    (depict-label-name markup-stream (symbol-type (tag-name tag)) (field-label field) :reference)
                                    (depict markup-stream " " :label-assign-10)
                                    (depict-break markup-stream 1)
                                    (depict-expression markup-stream world parameter %expr%))
                                  (depict-expression markup-stream world parameter %expr%))))
                          annotated-exprs
                          :indent 4
                          :prefix (if mutable :record-begin :tuple-begin)
                          :suffix (if mutable :record-end :tuple-end)
                          :separator ","
                          :break 1
                          :empty nil)))))
      
      (if mutable
        (depict-expr-parentheses (markup-stream level %prefix%)
          (depict-logical-block (markup-stream 4)
            (depict-semantic-keyword markup-stream 'new :after)
            (depict-tag-and-args markup-stream)))
        (depict-tag-and-args markup-stream)))))


; (& <label> <record-expr>)
(defun depict-& (markup-stream world level record-type label annotated-expr)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-expression markup-stream world annotated-expr %suffix%)
    (depict markup-stream ".")
    (depict-label-name markup-stream record-type label :reference)))


;;; Unions

(defun depict-in-or-not-in (markup-stream world level value-annotated-expr type type-expr op single-op)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-expression markup-stream world value-annotated-expr %term%)
    (depict-space markup-stream)
    (if (and (eq (type-kind type) :tag) (tag-keyword (type-tag type)))
      (progn
        (depict markup-stream single-op)
        (depict-space markup-stream)
        (depict-tag-name markup-stream (type-tag type) :reference))
      (progn
        (depict markup-stream op)
        (depict-space markup-stream)
        (depict-type-expr markup-stream world type-expr)))))

; (in <type> <expr>)
(defun depict-in (markup-stream world level value-annotated-expr type type-expr)
  (depict-in-or-not-in markup-stream world level value-annotated-expr type type-expr :member-10 "="))

; (not-in <type> <expr>)
(defun depict-not-in (markup-stream world level value-annotated-expr type type-expr)
  (depict-in-or-not-in markup-stream world level value-annotated-expr type type-expr :not-member-10 :not-equal))


;;; Writable Cells

; (writable-cell-of <element-type>)
(defun depict-writable-cell-of (markup-stream world level)
  (declare (ignore markup-stream world level))
  (error "No notation to creation of a writable cell"))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING STATEMENTS


(defmacro depict-statement-block (markup-stream &body body)
  `(depict-division-block (,markup-stream :statement '(:statement) '(:level))
     ,@body))


(defmacro depict-statement-block-using ((markup-stream paragraph-style) &body body)
  `(depict-division-block (,markup-stream ,paragraph-style (list :statement ,paragraph-style) '(:level))
     ,@body))


; Emit markup for the annotated statement.  The markup stream should be collecting divisions.
; If semicolon is true, depict a semicolon after the statement.
(defun depict-statement (markup-stream world semicolon last-paragraph-style annotated-stmt)
  (apply (get (first annotated-stmt) :depict-statement) markup-stream world semicolon last-paragraph-style (rest annotated-stmt)))


; If semicolon is true, depict a semicolon.
(defun depict-semicolon (markup-stream semicolon)
  (when semicolon
    (depict markup-stream ";")))


; Emit markup for the block of annotated statements indented by one level.  The markup stream
; should be collecting divisions.
; If semicolon is true, depict a semicolon after the statements.
(defun depict-statements (markup-stream world semicolon last-paragraph-style annotated-stmts)
  (depict-division-style (markup-stream :level)
    (if annotated-stmts
      (mapl #'(lambda (annotated-stmts)
                (depict-statement markup-stream
                                  world
                                  (or (rest annotated-stmts) semicolon)
                                  (if (rest annotated-stmts) :statement last-paragraph-style)
                                  (first annotated-stmts)))
            annotated-stmts)
      (depict-paragraph (markup-stream :statement)
        (depict-semantic-keyword markup-stream 'nothing nil)
        (depict-semicolon markup-stream semicolon)))))


; (exec <expr>)
(defun depict-exec (markup-stream world semicolon last-paragraph-style annotated-expr)
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-expression markup-stream world annotated-expr %expr%)
    (depict-semicolon markup-stream semicolon)))


; (const <name> <type> <value>)
; (var <name> <type> <value>)
(defun depict-var (markup-stream world semicolon last-paragraph-style name type-expr value-annotated-expr)
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-local-variable markup-stream name)
    (depict markup-stream ": ")
    (depict-type-expr markup-stream world type-expr)
    (depict markup-stream " " :assign-10)
    (depict-logical-block (markup-stream 6)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world value-annotated-expr %expr%)
      (depict-semicolon markup-stream semicolon))))


; (function (<name> (<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <result-type> . <statements>)
(defun depict-function (markup-stream world semicolon last-paragraph-style name-and-arg-binding-exprs result-type-expr &rest body-annotated-stmts)
  (depict-statement-block-using (markup-stream last-paragraph-style)
    (depict-paragraph (markup-stream :statement)
      (depict-semantic-keyword markup-stream 'proc :after)
      (depict-local-variable markup-stream (first name-and-arg-binding-exprs))
      (depict-function-signature markup-stream world (rest name-and-arg-binding-exprs) result-type-expr t))
    (depict-function-body markup-stream world semicolon last-paragraph-style body-annotated-stmts)))


; (<- <name> <value>)
(defun depict-<- (markup-stream world semicolon last-paragraph-style name value-annotated-expr global)
  (depict-paragraph (markup-stream last-paragraph-style)
    (if global
      (depict-global-variable markup-stream name :reference)
      (depict-local-variable markup-stream name))
    (depict markup-stream " " :assign-10)
    (depict-logical-block (markup-stream 6)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world value-annotated-expr %expr%)
      (depict-semicolon markup-stream semicolon))))


; (&= <record-expr> <value-expr>)
(defun depict-&= (markup-stream world semicolon last-paragraph-style record-type label record-annotated-expr value-annotated-expr)
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-& markup-stream world %unary% record-type label record-annotated-expr)
    (depict markup-stream " " :assign-10)
    (depict-logical-block (markup-stream 6)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world value-annotated-expr %expr%)
      (depict-semicolon markup-stream semicolon))))


; (action<- <action> <value>)
(defun depict-action<- (markup-stream world semicolon last-paragraph-style action-annotated-expr value-annotated-expr)
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-expression markup-stream world action-annotated-expr %expr%)
    (depict markup-stream " " :assign-10)
    (depict-logical-block (markup-stream 6)
      (depict-break markup-stream 1)
      (depict-expression markup-stream world value-annotated-expr %expr%)
      (depict-semicolon markup-stream semicolon))))


; (return [<value-expr>])
(defun depict-return (markup-stream world semicolon last-paragraph-style value-annotated-expr)
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-logical-block (markup-stream 4)
      (depict-semantic-keyword markup-stream 'return nil)
      (when value-annotated-expr
        (depict-space markup-stream)
        (depict-expression markup-stream world value-annotated-expr %expr%))
      (depict-semicolon markup-stream semicolon))))


; (cond (<condition-expr> . <statements>) ... (<condition-expr> . <statements>) [(nil . <statements>)])
(defun depict-cond (markup-stream world semicolon last-paragraph-style &rest annotated-cases)
  (assert-true (and annotated-cases (caar annotated-cases)))
  (depict-statement-block-using (markup-stream last-paragraph-style)
    (do ((annotated-cases annotated-cases (rest annotated-cases))
         (else nil t))
        ((endp annotated-cases))
      (let ((annotated-case (first annotated-cases)))
        (depict-statement-block markup-stream
          (depict-paragraph (markup-stream :statement)
            (let ((condition-annotated-expr (first annotated-case)))
              (if condition-annotated-expr
                (progn
                  (depict-semantic-keyword markup-stream (if else 'elsif 'if) :after)
                  (depict-logical-block (markup-stream 4)
                    (depict-expression markup-stream world condition-annotated-expr %expr%))
                  (depict-semantic-keyword markup-stream 'then :before))
                (depict-semantic-keyword markup-stream 'else nil))))
          (depict-statements markup-stream world nil :statement (rest annotated-case)))))
    (depict-paragraph (markup-stream last-paragraph-style)
      (depict-semantic-keyword markup-stream 'end :after)
      (depict-semantic-keyword markup-stream 'if nil)
      (depict-semicolon markup-stream semicolon))))


; (while <condition-expr> . <statements>)
(defun depict-while (markup-stream world semicolon last-paragraph-style condition-annotated-expr &rest loop-annotated-stmts)
  (depict-statement-block-using (markup-stream last-paragraph-style)
    (depict-statement-block markup-stream
      (depict-paragraph (markup-stream :statement)
        (depict-semantic-keyword markup-stream 'while :after)
        (depict-logical-block (markup-stream 4)
          (depict-expression markup-stream world condition-annotated-expr %expr%))
        (depict-semantic-keyword markup-stream 'do :before))
      (depict-statements markup-stream world nil :statement loop-annotated-stmts))
    (depict-paragraph (markup-stream last-paragraph-style)
      (depict-semantic-keyword markup-stream 'end :after)
      (depict-semantic-keyword markup-stream 'while nil)
      (depict-semicolon markup-stream semicolon))))


; (assert <condition-expr>)
(defun depict-assert (markup-stream world semicolon last-paragraph-style condition-annotated-expr)
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-logical-block (markup-stream 4)
      (depict-semantic-keyword markup-stream 'invariant :after)
      (depict-expression markup-stream world condition-annotated-expr %expr%)
      (depict-semicolon markup-stream semicolon))))


; (throw <value-expr>)
(defun depict-throw (markup-stream world semicolon last-paragraph-style value-annotated-expr)
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-logical-block (markup-stream 4)
      (depict-semantic-keyword markup-stream 'throw :after)
      (depict-expression markup-stream world value-annotated-expr %expr%)
      (depict-semicolon markup-stream semicolon))))


; (catch <body-statements> (<var> [:unused]) . <handler-statements>)
(defun depict-catch (markup-stream world semicolon last-paragraph-style body-annotated-stmts arg-binding-expr &rest handler-annotated-stmts)
  (depict-statement-block markup-stream
    (depict-paragraph (markup-stream :statement)
      (depict-semantic-keyword markup-stream 'try nil))
    (depict-statements markup-stream world nil :statement body-annotated-stmts))
  (depict-division-break markup-stream)
  (depict-statement-block markup-stream
    (depict-paragraph (markup-stream :statement)
      (depict-semantic-keyword markup-stream 'catch :after)
      (depict-logical-block (markup-stream 4)
        (depict-local-variable markup-stream (first arg-binding-expr))
        (depict markup-stream ": ")
        (depict-type-expr markup-stream world *semantic-exception-type-name*))
      (depict-semantic-keyword markup-stream 'do :before))
    (depict-statements markup-stream world nil :statement handler-annotated-stmts))
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-semantic-keyword markup-stream 'end :after)
    (depict-semantic-keyword markup-stream 'try nil)
    (depict-semicolon markup-stream semicolon)))


; (case <value-expr> (key <type> . <statements>) ... (keyword <type> . <statements>))
; where each key is one of:
;    :select    No special action
;    :narrow    Narrow the type of <value-expr>, which must be a variable, to this case's <type>
;    :otherwise Catch-all else case; <type> should be either nil or the remaining catch-all type
(defun depict-case (markup-stream world semicolon last-paragraph-style value-annotated-expr &rest annotated-cases)
  (depict-paragraph (markup-stream :statement)
    (depict-semantic-keyword markup-stream 'case :after)
    (depict-logical-block (markup-stream 8)
      (depict-expression markup-stream world value-annotated-expr %expr%))
    (depict-semantic-keyword markup-stream 'of :before))
  (depict-division-break markup-stream)
  (depict-division-style (markup-stream :level)
    (mapl #'(lambda (annotated-cases)
              (let ((annotated-case (car annotated-cases)))
                (depict-statement-block markup-stream
                  (depict-paragraph (markup-stream :statement)
                    (depict-logical-block (markup-stream 4)
                      (ecase (first annotated-case)
                        ((:select :narrow) (depict-type-expr markup-stream world (second annotated-case)))
                        ((:otherwise) (depict-semantic-keyword markup-stream 'else nil)))
                      (depict-semantic-keyword markup-stream 'do :before)))
                  (depict-statements markup-stream world (cdr annotated-cases) :statement (cddr annotated-case)))))
          annotated-cases))
  (depict-paragraph (markup-stream last-paragraph-style)
    (depict-semantic-keyword markup-stream 'end :after)
    (depict-semantic-keyword markup-stream 'case nil)
    (depict-semicolon markup-stream semicolon)))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING FUNCTIONS


(defun depict-function-signature (markup-stream world arg-binding-exprs result-type-expr show-type)
  (depict-logical-block (markup-stream 12)
    (depict-list markup-stream
                 #'(lambda (markup-stream arg-binding)
                     (depict-local-variable markup-stream (first arg-binding))
                     (depict markup-stream ": ")
                     (depict-type-expr markup-stream world (second arg-binding)))
                 arg-binding-exprs
                 :indent 2
                 :prefix "("
                 :suffix ")"
                 :separator ","
                 :break 1
                 :empty nil)
    (unless (or (eq result-type-expr 'void) (not show-type))
      (depict markup-stream ":")
      (depict-break markup-stream 1)
      (depict-type-expr markup-stream world result-type-expr))))


; Depict the signature of a lambda annotated expression.
(defun depict-lambda-signature (markup-stream world type-expr lambda-annotated-expr show-type)
  (declare (ignore type-expr))
  (assert-true (special-form-annotated-expr? world 'lambda lambda-annotated-expr))
  (depict-function-signature markup-stream world (third lambda-annotated-expr) (fourth lambda-annotated-expr) show-type))


; Return the list of body annotated statements of a lambda annotated expression.
(defun lambda-body-annotated-stmts (lambda-annotated-expr)
  (cddddr lambda-annotated-expr))


; Return true if the function body given by body-annotated-stmts is a single return statement.
(defun function-body-is-expression? (world body-annotated-stmts)
  (and *depict-trivial-functions-as-expressions*
       body-annotated-stmts
       (endp (cdr body-annotated-stmts))
       (special-form-annotated-stmt? world 'return (first body-annotated-stmts))
       (cdr (first body-annotated-stmts))))


; Depict a function's body as a series of divisions.  markup-stream should be accepting divisions.
; If semicolon is true, depict a semicolon.
; Use last-paragraph-style for depicting the last paragraph.
(defun depict-function-body (markup-stream world semicolon last-paragraph-style body-annotated-stmts)
  (if (function-body-is-expression? world body-annotated-stmts)
    (depict-division-style (markup-stream :level)
      (depict-paragraph (markup-stream last-paragraph-style)
        (depict markup-stream :identical-10 " ")
        (depict-expression markup-stream world (second (first body-annotated-stmts)) %expr%)
        (depict-semicolon markup-stream semicolon)))
    (progn
      (depict-division-break markup-stream)
      (when body-annotated-stmts
        (depict-statements markup-stream world nil :statement body-annotated-stmts))
      (depict-paragraph (markup-stream last-paragraph-style)
        (depict-semantic-keyword markup-stream 'end :after)
        (depict-semantic-keyword markup-stream 'proc nil)
        (depict-semicolon markup-stream semicolon)))))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING COMMANDS


(defmacro depict-semantics ((markup-stream depict-env &optional (paragraph-style :algorithm-stmt)) &body body)
  `(when (depict-mode ,markup-stream ,depict-env :semantics)
     (depict-division-style (,markup-stream :nowrap)
       (depict-paragraph (,markup-stream ,paragraph-style)
         ,@body))))


(defmacro depict-algorithm ((markup-stream depict-env &optional (division-style :algorithm)) &body body)
  `(when (depict-mode ,markup-stream ,depict-env :semantics)
     (depict-division-style (,markup-stream :nowrap)
       (depict-division-style (,markup-stream ,division-style)
         ,@body))))


; (%highlight <highlight> <command> ... <command>)
; Depict the commands highlighted with the <highlight> division style.
(defun depict-%highlight (markup-stream world depict-env highlight &rest commands)
  (when commands
    (depict-division-style (markup-stream highlight t)
      (depict-commands markup-stream world depict-env commands))))


(defun depict-text-paragraph (markup-stream world depict-env paragraph-style text)
  (depict-paragraph (markup-stream paragraph-style)
    (let ((grammar-info (depict-env-grammar-info depict-env))
          (*styled-text-world* world))
      (if grammar-info
        (let ((*styled-text-grammar-parametrization* (grammar-info-grammar grammar-info)))
          (depict-styled-text markup-stream text))
        (depict-styled-text markup-stream text)))))


; (%heading <level> . <styled-text>)
; (%heading (<level <mode>) . <styled-text>)
; <mode> is one of:
;   :syntax     This is a comment about the syntax
;   :semantics  This is a comment about the semantics (not displayed when semantics are not displayed)
;   nil         This is a general comment
(defun depict-%heading (markup-stream world depict-env level-mode &rest text)
  (let ((level level-mode)
        (mode nil))
    (unless (integerp level-mode)
      (unless (structured-type? level-mode '(tuple integer symbol))
        (error "~S should be either <level> or (<level> <mode>)" level-mode))
      (setq level (first level-mode))
      (setq mode (second level-mode)))
    (let* ((heading-level (+ level (depict-env-heading-offset depict-env)))
           (heading-style (svref #(:heading1 :heading2 :heading3 :heading4 :heading5 :heading6) (1- heading-level))))
      (when (quiet-depict-mode depict-env mode)
        (depict-text-paragraph markup-stream world depict-env heading-style text)))))


; (%text <mode> . <styled-text>)
; <mode> is one of:
;   :syntax     This is a comment about the syntax
;   :semantics  This is a comment about the semantics (not displayed when semantics are not displayed)
;   :comment    This is a comment about the following piece of semantics (not displayed when semantics are not displayed)
;   nil         This is a general comment
(defun depict-%text (markup-stream world depict-env mode &rest text)
  (when (depict-mode markup-stream depict-env (if (eq mode :comment) :semantics mode))
    (depict-text-paragraph markup-stream world depict-env (if (eq mode :comment) :semantic-comment :body-text) text)))


; (grammar-argument <argument> <attribute> <attribute> ... <attribute>)
(defun depict-grammar-argument (markup-stream world depict-env argument &rest attributes)
  (declare (ignore world))
  (let ((seen-grammar-arguments (depict-env-seen-grammar-arguments depict-env))
        (abbreviated-argument (symbol-abbreviation argument)))
    (unless (gethash abbreviated-argument seen-grammar-arguments)
      (when (depict-mode markup-stream depict-env :syntax)
        (depict-paragraph (markup-stream :grammar-argument)
          (depict-nonterminal-argument markup-stream argument)
          (depict markup-stream " " :member-10 " ")
          (depict-list markup-stream
                       #'(lambda (markup-stream attribute)
                           (depict-nonterminal-attribute markup-stream attribute))
                       attributes
                       :prefix "{"
                       :suffix "}"
                       :separator ", ")))
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
      (unless (or (hidden-nonterminal? general-nonterminal)
                  (every #'seen-nonterminal? (general-grammar-symbol-instances grammar general-nonterminal)))
        (let ((visible (depict-mode markup-stream depict-env :syntax)))
          (dolist (general-rule (grammar-general-rules grammar general-nonterminal))
            (let ((rule-lhs-nonterminals (general-grammar-symbol-instances grammar (general-rule-lhs general-rule))))
              (unless (every #'seen-nonterminal? rule-lhs-nonterminals)
                (when (some #'seen-nonterminal? rule-lhs-nonterminals)
                  (warn "General rule for ~S listed before specific ones; use %rule to disambiguate" general-nonterminal))
                (when visible
                  (depict-general-rule markup-stream general-rule (grammar-highlights grammar)))
                (dolist (nonterminal rule-lhs-nonterminals)
                  (setf (gethash nonterminal seen-nonterminals) t))))))))))
;******** May still have a problem when a specific rule precedes a general one.


; (%charclass <nonterminal>)
(defun depict-%charclass (markup-stream world depict-env nonterminal-source)
  (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
         (grammar (grammar-info-grammar grammar-info))
         (nonterminal (grammar-parametrization-intern grammar nonterminal-source))
         (charclass (grammar-info-charclass grammar-info nonterminal)))
    (unless charclass
      (error "%charclass with a non-charclass ~S" nonterminal))
    (if (gethash nonterminal (depict-env-seen-nonterminals depict-env))
      (warn "Duplicate charclass ~S" nonterminal)
      (progn
        (when (depict-mode markup-stream depict-env :syntax)
          (depict-charclass markup-stream charclass)
          (dolist (action-cons (charclass-actions charclass))
            (depict-charclass-action markup-stream world depict-env (car action-cons) (cdr action-cons) nonterminal)))
        (setf (gethash nonterminal (depict-env-seen-nonterminals depict-env)) t)))))


; (%print-actions (<string> <action-name> ... <action-name>) ... (<string> <action-name> ... <action-name>))
(defun depict-%print-actions (markup-stream world depict-env &rest action-groups)
  (declare (ignore world))
  (let ((pending-actions (nreverse (depict-env-pending-actions-reverse depict-env))))
    (setf (depict-env-pending-actions-reverse depict-env) nil)
    (dolist (action-group action-groups)
      (assert-type action-group (cons string (list identifier)))
      (let ((group-name (car action-group))
            (action-names (cdr action-group)))
        (when (some #'(lambda (pending-action) (member (car pending-action) action-names)) pending-actions)
          (when (depict-semantic-group markup-stream depict-env group-name)
            (setq pending-actions
                  (mapcan #'(lambda (pending-action)
                              (if (member (car pending-action) action-names)
                                (progn
                                  (funcall (cdr pending-action) markup-stream depict-env)
                                  nil)
                                (list pending-action)))
                          pending-actions))))))
    (dolist (pending-action pending-actions)
      (funcall (cdr pending-action) markup-stream depict-env))
    (assert-true (null (depict-env-pending-actions-reverse depict-env)))))


; (deftag <name>)
(defun depict-deftag (markup-stream world depict-env name)
  (depict-semantics (markup-stream depict-env)
    (depict-logical-block (markup-stream 2)
      (let ((tag (scan-tag world name)))
        (depict-semantic-keyword markup-stream 'tag :after)
        (depict-tag-name markup-stream tag :definition))
      (depict markup-stream ";"))))


; (deftuple <name> (<name1> <type1>) ... (<namen> <typen>))
; (defrecord <name> (<name1> <type1>) ... (<namen> <typen>))
(defun depict-deftuple (markup-stream world depict-env name &rest fields)
  (let* ((type (scan-kinded-type world name :tag))
         (tag (type-tag type))
         (mutable (tag-mutable tag))
         (keyword (if mutable 'record 'tuple)))
    (depict-algorithm (markup-stream depict-env)
      (depict-paragraph (markup-stream :statement)
        (depict-semantic-keyword markup-stream keyword :after)
        (depict-type-name markup-stream name :definition))
      (depict-division-style (markup-stream :level)
        (mapl #'(lambda (fields)
                  (let ((field (car fields)))
                    (depict-paragraph (markup-stream :statement)
                      (depict-logical-block (markup-stream 4)
                        (depict-label-name markup-stream type (first field) nil)
                        (depict markup-stream ": ")
                        (depict-type-expr markup-stream world (second field) %%type%%)
                        (when (cdr fields)
                          (depict markup-stream ","))))))
              fields))
      (depict-paragraph (markup-stream :statement-last)
        (depict-semantic-keyword markup-stream 'end :after)
        (depict-semantic-keyword markup-stream keyword nil)
        (depict markup-stream ";")))))


; (deftype <name> <type>)
(defun depict-deftype (markup-stream world depict-env name type-expr)
  (depict-semantics (markup-stream depict-env)
    (depict-logical-block (markup-stream 2)
      (depict-type-name markup-stream name :definition)
      (depict-break markup-stream 1)
      (depict-logical-block (markup-stream 4)
        (depict markup-stream "= ")
        (depict-type-expr markup-stream world type-expr))
      (depict markup-stream ";"))))


(defun depict-colon-and-type (markup-stream world type-expr)
  (depict-logical-block (markup-stream 4)
    (depict markup-stream ":")
    (depict-break markup-stream 1)
    (depict-type-expr markup-stream world type-expr)))


(defun depict-equals-and-value (markup-stream world value-annotated-expr assignment)
  (depict-break markup-stream 1)
  (depict-logical-block (markup-stream 4)
    (depict markup-stream assignment " ")
    (depict-expression markup-stream world value-annotated-expr %expr%)
    (depict markup-stream ";")))


(defun depict-begin (markup-stream world value-annotated-expr last-paragraph-style)
  (assert-true (eq (car value-annotated-expr) 'expr-annotation:begin))
  (depict-division-style (markup-stream :level)
    (depict-paragraph (markup-stream :statement)
      (depict-semantic-keyword markup-stream 'begin nil))
    (let ((annotated-stmts (cdr value-annotated-expr)))
      (when annotated-stmts
        (depict-statements markup-stream world nil :statement annotated-stmts)))
    (depict-paragraph (markup-stream last-paragraph-style)
      (depict-semantic-keyword markup-stream 'end nil)
      (depict markup-stream ";"))))


; (define <name> <type> <value>)
(defun depict-define (markup-stream world depict-env name type-expr value-expr)
  (let ((value-annotated-expr (nth-value 2 (scan-value world *null-type-env* value-expr))))
    (if (eq (car value-annotated-expr) 'expr-annotation:begin)
      (depict-algorithm (markup-stream depict-env)
        (depict-paragraph (markup-stream :statement)
          (depict-logical-block (markup-stream 0)
            (depict-global-variable markup-stream name :definition)
            (depict-colon-and-type markup-stream world type-expr)))
        (depict-begin markup-stream world value-annotated-expr :statement-last))
      (depict-semantics (markup-stream depict-env)
        (depict-logical-block (markup-stream 0)
          (depict-global-variable markup-stream name :definition)
          (depict-colon-and-type markup-stream world type-expr)
          (depict-equals-and-value markup-stream world value-annotated-expr "="))))))


; (defvar <name> <type> <value>)
(defun depict-defvar (markup-stream world depict-env name type-expr value-expr)
  (let ((value-annotated-expr (nth-value 2 (scan-value world *null-type-env* value-expr))))
    (if (eq (car value-annotated-expr) 'expr-annotation:begin)
      (error "defvar shouldn't use begin")
      (depict-semantics (markup-stream depict-env)
        (depict-logical-block (markup-stream 0)
          (depict-global-variable markup-stream name :definition)
          (depict-colon-and-type markup-stream world type-expr)
          (depict-equals-and-value markup-stream world value-annotated-expr :assign-10))))))


; (defun <name> (-> (<type1> ... <typen>) <result-type>) (lambda ((<arg1> <type1>) ... (<argn> <typen>)) <result-type> . <statements>))
(defun depict-defun (markup-stream world depict-env name type-expr value-expr)
  (assert-true (eq (first type-expr) '->))
  (let* ((value-annotated-expr (nth-value 2 (scan-value world *null-type-env* value-expr)))
         (body-annotated-stmts (lambda-body-annotated-stmts value-annotated-expr)))
    (depict-algorithm (markup-stream depict-env)
      (depict-statement-block-using (markup-stream :statement-last)
        (depict-paragraph (markup-stream :statement)
          (depict-semantic-keyword markup-stream 'proc :after)
          (depict-global-variable markup-stream name :definition)
          (depict-lambda-signature markup-stream world type-expr value-annotated-expr t))
        (depict-function-body markup-stream world t :statement-last body-annotated-stmts)))))



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
                      (eq nonterminal *start-nonterminal*)
                      (hidden-nonterminal? nonterminal))
            (push nonterminal missed-nonterminals)))
        (when missed-nonterminals
          (warn "Nonterminals not printed: ~S" missed-nonterminals)))
      (setf (depict-env-grammar-info depict-env) nil)
      (setf (depict-env-seen-nonterminals depict-env) nil)
      (setf (depict-env-seen-grammar-arguments depict-env) nil))))


(defmacro depict-delayed-action ((markup-stream depict-env action-name) &body depictor)
  (let ((saved-division-style (gensym "SAVED-DIVISION-STYLE")))
    `(let ((,saved-division-style (save-division-style ,markup-stream)))
       (push (cons ,action-name
                   #'(lambda (,markup-stream ,depict-env)
                       (with-saved-division-style (,markup-stream ,saved-division-style t) ,@depictor)))
             (depict-env-pending-actions-reverse ,depict-env)))))


(defun depict-action-name-and-symbol (markup-stream action-name general-grammar-symbol)
  (depict-action-name markup-stream action-name)
  (depict markup-stream :action-begin)
  (depict-general-grammar-symbol markup-stream general-grammar-symbol :reference)
  (depict markup-stream :action-end))


(defun depict-declare-action-contents (markup-stream world action-name general-grammar-symbol type-expr)
  (depict-action-name-and-symbol markup-stream action-name general-grammar-symbol)
  (depict-logical-block (markup-stream 2)
    (depict markup-stream ":")
    (depict-break markup-stream 1)
    (depict-type-expr markup-stream world type-expr)))


; (declare-action <action-name> <general-grammar-symbol> <type> <mode> <parameter-list> <command> ... <command>)
; <mode> is one of:
;    :hide      Don't depict this action declaration because it's for a hidden production;
;    :singleton Don't depict this action declaration because it contains a singleton production;
;    :action    Depict this action declaration; all corresponding actions will be depicted by depict-action;
;    :actfun    Depict this action declaration; all corresponding actions will be depicted by depict-actfun;
;    :writable  Depict this action declaration but not actions.
; <parameter-list> contains the names of the action parameters when <mode> is :actfun.
(defun depict-declare-action (markup-stream world depict-env action-name general-grammar-symbol-source type-expr mode parameter-list &rest commands)
  (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
         (general-grammar-symbol (grammar-parametrization-intern (grammar-info-grammar grammar-info) general-grammar-symbol-source)))
    (unless (and (general-nonterminal? general-grammar-symbol) (hidden-nonterminal? general-grammar-symbol))
      (ecase mode
        (:hide)
        (:singleton (depict-delayed-action (markup-stream depict-env action-name)
                      (depict-algorithm (markup-stream depict-env)
                        (depict-commands markup-stream world depict-env commands))))
        ((:action :actfun)
         (depict-delayed-action (markup-stream depict-env action-name)
           (depict-algorithm (markup-stream depict-env)
             (depict-paragraph (markup-stream :statement)
               (if (eq mode :actfun)
                 (depict-logical-block (markup-stream 0)
                   (depict-semantic-keyword markup-stream 'proc :after)
                   (depict-action-name-and-symbol markup-stream action-name general-grammar-symbol)
                   (depict-break markup-stream 1)
                   (unless (and (consp type-expr) (eq (first type-expr) '->))
                     (error "Destructuring requires ~S to be a -> type" type-expr))
                   (let ((->-parameters (second type-expr))
                         (->-result (third type-expr)))
                     (unless (= (length ->-parameters) (length parameter-list))
                       (error "Parameter count mistmatch: ~S and ~S" ->-parameters parameter-list))
                     (let ((bindings (mapcar #'list parameter-list ->-parameters)))
                       (depict-function-signature markup-stream world bindings ->-result t))))
                 (progn
                   (depict-declare-action-contents markup-stream world action-name general-grammar-symbol type-expr)
                   (depict markup-stream ";"))))
             (depict-commands markup-stream world depict-env commands)
             (when (eq mode :actfun)
               (depict-paragraph (markup-stream :statement-last)
                 (depict-semantic-keyword markup-stream 'end :after)
                 (depict-semantic-keyword markup-stream 'proc nil)
                 (depict markup-stream ";"))))))
        (:writable
         (depict-delayed-action (markup-stream depict-env action-name)
           (depict-semantics (markup-stream depict-env)
             (depict-declare-action-contents markup-stream world action-name general-grammar-symbol type-expr)
             (depict markup-stream ";"))))))))


; Declare and define the lexer-action on the charclass given by nonterminal.
(defun depict-charclass-action (markup-stream world depict-env action-name lexer-action nonterminal)
  (unless (default-action? action-name)
    (depict-delayed-action (markup-stream depict-env action-name)
      (depict-semantics (markup-stream depict-env)
        (depict-logical-block (markup-stream 4)
          (depict-declare-action-contents markup-stream world action-name nonterminal (lexer-action-type-expr lexer-action))
          (depict-break markup-stream 1)
          (depict-logical-block (markup-stream 3)
            (depict markup-stream "= ")
            (depict-global-variable markup-stream (lexer-action-function-name lexer-action) :external)
            (depict markup-stream "(")
            (depict-general-nonterminal markup-stream nonterminal :reference)
            (depict markup-stream ")"))
          (depict markup-stream ";"))))))


; (action <action-name> <production-name> <type> <mode> <value>)
; <mode> is one of:
;    :hide      Don't depict this action;
;    :singleton Depict this action along with its declaration;
;    :first     Depict this action, which is the first in the rule
;    :middle    Depict this action, which is neither the first nor the last in the rule
;    :last      Depict this action, which is the last in the rule
(defun depict-action (markup-stream world depict-env action-name production-name type-expr mode value-expr)
  (depict-general-action markup-stream world depict-env action-name production-name type-expr mode value-expr nil))

; (actfun <action-name> <production-name> (-> (<type1> ... <typen>) <result-type>) <mode>
;    (lambda ((<arg1> <type1>) ... (<argn> <typen>)) <result-type> . <statements>))
; <mode> is one of:
;    :hide      Don't depict this action;
;    :singleton Depict this action along with its declaration;
;    :first     Depict this action, which is the first in the rule
;    :middle    Depict this action, which is neither the first nor the last in the rule
;    :last      Depict this action, which is the last in the rule
(defun depict-actfun (markup-stream world depict-env action-name production-name type-expr mode value-expr)
  (depict-general-action markup-stream world depict-env action-name production-name type-expr mode value-expr t))

(defun depict-action-signature (markup-stream action-name general-production action-grammar-symbols)
  (depict-action-name markup-stream action-name)
  (depict markup-stream :action-begin)
  (depict-general-production markup-stream general-production :reference action-grammar-symbols)
  (depict markup-stream :action-end))

(defun depict-general-action (markup-stream world depict-env action-name production-name type-expr mode value-expr destructured)
  (unless (eq mode :hide)
    (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
           (grammar (grammar-info-grammar grammar-info))
           (general-production (grammar-general-production grammar production-name))
           (lhs (general-production-lhs general-production)))
      (unless (hidden-nonterminal? lhs)
        (let* ((initial-env (general-production-action-env grammar general-production t))
               (type (scan-type world type-expr))
               (value-annotated-expr (nth-value 1 (scan-typed-value-or-begin world initial-env value-expr type)))
               (action-grammar-symbols (annotated-expr-grammar-symbols value-annotated-expr))
               (mode-style (cdr (assert-non-null (assoc mode '((:singleton . nil) (:first . :level-wide) (:middle . :level-wide) (:last . :level)))))))
          (depict-division-style (markup-stream mode-style)
            (if destructured
              (let* ((body-annotated-stmts (lambda-body-annotated-stmts value-annotated-expr))
                     (semicolon (not (eq mode :last)))
                     (last-paragraph-style (if semicolon :statement-last :statement)))
                (depict-statement-block-using (markup-stream last-paragraph-style)
                  (if (eq mode :singleton)
                    (progn
                      (depict-paragraph (markup-stream :statement)
                        (depict-logical-block (markup-stream 0)
                          (depict-semantic-keyword markup-stream 'proc :after)
                          (depict-action-signature markup-stream action-name general-production action-grammar-symbols)
                          (depict-break markup-stream 1)
                          (depict-lambda-signature markup-stream world type-expr value-annotated-expr t)))
                      (depict-function-body markup-stream world semicolon last-paragraph-style body-annotated-stmts))
                    (progn
                      (depict-paragraph (markup-stream :statement)
                        (depict markup-stream :action-begin)
                        (depict-general-production markup-stream general-production :reference action-grammar-symbols)
                        (depict markup-stream :action-end)
                        (depict-semantic-keyword markup-stream 'do :before))
                      (depict-statements markup-stream world semicolon last-paragraph-style body-annotated-stmts)))))
              
              (let ((last-paragraph-style (if (member mode '(:singleton :last)) :statement-last :statement)))
                (if (eq (car value-annotated-expr) 'expr-annotation:begin)
                  (progn
                    (depict-paragraph (markup-stream :statement)
                      (depict-logical-block (markup-stream 0)
                        (depict-action-signature markup-stream action-name general-production action-grammar-symbols)
                        (when (eq mode :singleton)
                          (depict-colon-and-type markup-stream world type-expr))))
                    (depict-begin markup-stream world value-annotated-expr last-paragraph-style))
                  
                  (depict-paragraph (markup-stream last-paragraph-style)
                    (depict-logical-block (markup-stream 0)
                      (depict-action-signature markup-stream action-name general-production action-grammar-symbols)
                      (when (eq mode :singleton)
                        (depict-colon-and-type markup-stream world type-expr))
                      (depict-equals-and-value markup-stream world value-annotated-expr "="))))))))))))


; (terminal-action <action-name> <terminal> <lisp-function>)
(defun depict-terminal-action (markup-stream world depict-env action-name terminal function)
  (declare (ignore markup-stream world depict-env action-name terminal function)))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING STYLED TEXT

; Styled text can include the formats below as long as *styled-text-world* is bound around the call
; to depict-styled-text.

; (:type <type-expression>)
(defun depict-styled-text-type (markup-stream type-expression)
  (depict-type-expr markup-stream *styled-text-world* type-expression))

(setf (styled-text-depictor :type) #'depict-styled-text-type)


; (:tag <name>)
(defun depict-styled-text-tag (markup-stream tag-name)
  (depict-tag-name markup-stream (scan-tag *styled-text-world* tag-name) :reference))

(setf (styled-text-depictor :tag) #'depict-styled-text-tag)


; (:label <type-name> <label>)
(defun depict-styled-text-label (markup-stream type-name label)
  (let ((type (scan-type *styled-text-world* type-name)))
    (depict-label-name markup-stream type label :reference)))

(setf (styled-text-depictor :label) #'depict-styled-text-label)


; (:global <name> [<link>])
(defun depict-styled-text-global-variable (markup-stream name &optional (link :reference))
  (let ((interned-name (world-find-symbol *styled-text-world* name)))
    (if (and interned-name (symbol-primitive interned-name))
      (depict-primitive markup-stream (symbol-primitive interned-name))
      (progn
        (unless (symbol-type interned-name)
          (warn "~A is depicted as a global variable but isn't one" name))
        (depict-global-variable markup-stream name link)))))

(setf (styled-text-depictor :global) #'depict-styled-text-global-variable)


; (:global-call <global-name> <local-name> ... <local-name>)
(defun depict-styled-text-global-call (markup-stream global-name &rest local-names)
  (depict-styled-text-global-variable markup-stream global-name nil)
  (depict markup-stream "(")
  (when local-names
    (depict-local-variable markup-stream (first local-names))
    (dolist (local-name (rest local-names))
      (depict markup-stream "," :nbsp)
      (depict-local-variable markup-stream local-name)))
  (depict markup-stream ")"))

(setf (styled-text-depictor :global-call) #'depict-styled-text-global-call)


; (:local <name>)
(setf (styled-text-depictor :local) #'depict-local-variable)


; (:constant <value>)
; <value> can be either an integer, a float, a character, or a string.
(setf (styled-text-depictor :constant) #'depict-constant)


; (:action <name>)
(setf (styled-text-depictor :action) #'depict-action-name)

