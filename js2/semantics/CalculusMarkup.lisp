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
  '(not and or is type oneof tuple action function if then else throw try catch in new case of end let letexc))

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
(defstruct (depict-env (:constructor make-depict-env (visible-semantics)))
  (visible-semantics t :type bool)                               ;Nil if semantics are not to be depicted
  (grammar-info nil :type (or null grammar-info))                ;The current grammar-info or nil if none
  (seen-nonterminals nil :type (or null hash-table))             ;Hash table (nonterminal -> t) of nonterminals already depicted
  (seen-grammar-arguments nil :type (or null hash-table))        ;Hash table (grammar-argument -> t) of grammar-arguments already depicted
  (mode nil :type (member nil :syntax :semantics))               ;Current heading (:syntax or :semantics) or nil if none
  (pending-actions-reverse nil :type list))                      ;Reverse-order list of closures of actions pending for a %print-actions


(defun checked-depict-env-grammar-info (depict-env)
  (or (depict-env-grammar-info depict-env)
      (error "Grammar needed")))


; Set the mode to the given mode, emitting a heading if necessary.
; Return true if the contents should be visible, nil if not.
(defun depict-mode (markup-stream depict-env mode)
  (unless (eq mode (depict-env-mode depict-env))
    (when (depict-env-visible-semantics depict-env)
      (ecase mode
        (:syntax (depict-paragraph (markup-stream ':grammar-header)
                   (depict markup-stream "Syntax")))
        (:semantics (depict-paragraph (markup-stream ':grammar-header)
                      (depict markup-stream "Semantics")))
        ((nil))))
    (setf (depict-env-mode depict-env) mode))
  (or (depict-env-visible-semantics depict-env)
      (not (eq mode :semantics))))


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
(defun depict-world-commands (markup-stream world &key (visible-semantics t))
  (let ((depict-env (make-depict-env visible-semantics)))
    (depict-commands markup-stream world depict-env (world-commands-source world))
    (depict-clear-grammar markup-stream world depict-env)))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPICTING TYPES

;;; The level argument indicates what kinds of component types may be represented without being placed
;;; in parentheses.

(defparameter *type-level* (make-partial-order))
(def-partial-order-element *type-level* %%primary%%)                              ;id, oneof, tuple, (type), {type}
(def-partial-order-element *type-level* %%suffix%% %%primary%%)                   ;type[], type^
(def-partial-order-element *type-level* %%function%% %%suffix%%)                  ;type x type -> type
(defparameter %%max%% %%function%%)


; Emit markup for the name of a type, which must be a symbol.
; link should be one of:
;   :reference   if this is a reference of this type name;
;   :external    if this is an external reference of this type name;
;   :definition  if this is a definition of this type name;
;   nil          if this use of the type name should not be cross-referenced.
(defun depict-type-name (markup-stream type-name link)
  (let ((name (symbol-upper-mixed-case-name type-name)))
    (depict-link (markup-stream link "T-" name nil)
      (depict-char-style (markup-stream :type-name)
        (depict markup-stream name)))))


; Emit markup for the name of a tuple or oneof field, which must be a symbol.
; link should be one of:
;   :reference   if this is a reference of this general-nonterminal;
;   :external    if this is an external reference of this general-nonterminal;
;   :definition  if this is a definition of this general-nonterminal;
;   nil          if this use of the general-nonterminal should not be cross-referenced.
; type is the tuple or oneof type that contains the field or nil if not known
; (it's only needed if link is :reference or :external).
(defun depict-field-name (markup-stream field-name link &optional type)
  (labels
    ((depict-it (markup-stream)
       (depict-char-style (markup-stream :field-name)
         (depict markup-stream (symbol-lower-mixed-case-name field-name)))))
    (if (or (eq link :reference) (eq link :external))
      (let ((type-name (type-name type)))
        (assert-true (field-type type field-name))
        (if type-name
          (depict-link (markup-stream link "T-" (symbol-upper-mixed-case-name type-name) nil)
            (depict-it markup-stream))
          (progn
            (warn "Reference to field ~S of anonymous type ~S" field-name type)
            (depict-it markup-stream))))
      (depict-it markup-stream))))


; If level < threshold, depict an opening parenthesis, evaluate body, and depict a closing
; parentheses.  Otherwise, just evaluate body.
; Return the result value of body.
(defmacro depict-type-parentheses ((markup-stream level threshold) &body body)
  `(depict-optional-parentheses (,markup-stream (partial-order-< ,level ,threshold))
     ,@body))


; Emit markup for the given type expression.  level is non-nil if this is a recursive
; call to depict-type-expr for which the markup-stream's style is :type-expression.
; In this case level indicates the binding level imposed by the enclosing type expression.
(defun depict-type-expr (markup-stream world type-expr &optional level)
  (cond
   ((identifier? type-expr)
    (let ((type-name (world-intern world type-expr)))
      (depict-type-name markup-stream type-expr (if (symbol-type-user-defined type-name) :reference :external))))
   ((type? type-expr)
    (let ((type-str (print-type-to-string type-expr)))
      (warn "Depicting raw type ~A" type-str)
      (depict markup-stream "<<<" type-str ">>>")))
   (t (let ((depictor (get (world-intern world (first type-expr)) :depict-type-constructor)))
        (if level
          (apply depictor markup-stream world level (rest type-expr))
          (depict-char-style (markup-stream :type-expression)
            (apply depictor markup-stream world %%max%% (rest type-expr))))))))


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
    (depict-type-expr markup-stream world result-type-expr %%suffix%%)))


; (vector <element-type>)
;   "<element-type>[]"
(defun depict-vector (markup-stream world level element-type-expr)
  (depict-type-parentheses (markup-stream level %%suffix%%)
    (depict-type-expr markup-stream world element-type-expr %%suffix%%)
    (depict markup-stream "[]")))


; (set <element-type>)
;   "{<element-type>}"
(defun depict-set (markup-stream world level element-type-expr)
  (declare (ignore level))
  (depict markup-stream "{")
  (depict-type-expr markup-stream world element-type-expr %%max%%)
  (depict markup-stream "}"))


; (address <element-type>)
;   "<element-type>^"
(defun depict-address (markup-stream world level element-type-expr)
  (depict-type-parentheses (markup-stream level %%suffix%%)
    (depict-type-expr markup-stream world element-type-expr %%suffix%%)
    (depict markup-stream :up-arrow-10)))


(defun depict-tuple-or-oneof (markup-stream world keyword-symbol tag-pairs)
  (depict-semantic-keyword markup-stream keyword-symbol)
  (depict-list
   markup-stream
   #'(lambda (markup-stream tag-pair)
       (if (identifier? tag-pair)
         (depict-field-name markup-stream tag-pair :definition)
         (progn
           (depict-field-name markup-stream (first tag-pair) :definition)
           (depict markup-stream ": ")
           (depict-type-expr markup-stream world (second tag-pair) %%max%%))))
   tag-pairs
   :indent 6
   :prefix " {"
   :prefix-break 0
   :suffix "}"
   :separator ";"
   :break 1
   :empty nil))

; (oneof (<tag1> <type1>) ... (<tagn> <typen>))
;   "ONEOF{<tag1>: <type1>; ...; <tagn>:<typen>}"
(defun depict-oneof (markup-stream world level &rest tags-and-types)
  (declare (ignore level))
  (depict-tuple-or-oneof markup-stream world 'oneof tags-and-types))

; (tuple (<tag1> <type1>) ... (<tagn> <typen>))
;   "TUPLE{<tag1>: <type1>; ...; <tagn>:<typen>}"
(defun depict-tuple (markup-stream world level &rest tags-and-types)
  (declare (ignore level))
  (depict-tuple-or-oneof markup-stream world 'tuple tags-and-types))


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
    (depict-expr-parentheses (markup-stream level %suffix%)
      (depict-annotated-value-expr markup-stream world annotated-function-expr %suffix%)
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


; Emit markup for the given annotated value expression.  level indicates the binding level imposed
; by the enclosing expression.
(defun depict-annotated-value-expr (markup-stream world annotated-expr &optional (level %expr%))
  (let ((annotation (first annotated-expr))
        (args (rest annotated-expr)))
    (ecase annotation
      (expr-annotation:constant (depict-constant markup-stream (first args)))
      (expr-annotation:primitive (depict-primitive markup-stream (symbol-primitive (first args))))
      (expr-annotation:local (depict-local-variable markup-stream (first args)))
      (expr-annotation:global (depict-global-variable markup-stream (first args) :reference))
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
     (when (partial-order-< level %stmt%)
       (depict-break ,markup-stream))
     (depict-expr-parentheses (,markup-stream level %stmt%)
       (depict-semantic-keyword ,markup-stream ,keyword)
       ,@(and space `((depict-space ,markup-stream)))
       ,@body)))


; (bottom)
(defun depict-bottom (markup-stream world level)
  (declare (ignore world level))
  (depict markup-stream ':bottom-10))


(defun depict-function-bindings (markup-stream world arg-binding-exprs)
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

; (function ((<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <body>)
(defun depict-function (markup-stream world level arg-binding-exprs body-annotated-expr)
  (depict-statement (markup-stream 'function nil)
    (depict-function-bindings markup-stream world arg-binding-exprs)
    (depict-logical-block (markup-stream 4)
      (depict-break markup-stream)
      (depict-annotated-value-expr markup-stream world body-annotated-expr %stmt%))))


; (if <condition-expr> <true-expr> <false-expr>)
(defun depict-if (markup-stream world level condition-annotated-expr true-annotated-expr false-annotated-expr)
  (depict-statement (markup-stream 'if)
    (depict-logical-block (markup-stream 4)
      (depict-annotated-value-expr markup-stream world condition-annotated-expr))
    (depict-break markup-stream)
    (depict-semantic-keyword markup-stream 'then)
    (depict-space markup-stream)
    (depict-logical-block (markup-stream 7)
      (depict-annotated-value-expr markup-stream world true-annotated-expr %stmt%))
    (depict-break markup-stream)
    (depict-semantic-keyword markup-stream 'else)
    (depict-space markup-stream)
    (depict-logical-block (markup-stream (if (special-form-annotated-expr? 'if false-annotated-expr) nil 6))
      (depict-annotated-value-expr markup-stream world false-annotated-expr %stmt%))))


; (throw <value-expr>)
(defun depict-throw (markup-stream world level value-annotated-expr)
  (depict-statement (markup-stream 'throw)
    (depict-logical-block (markup-stream 4)
      (depict-annotated-value-expr markup-stream world value-annotated-expr))))


; (catch <body> (<var> [:unused]) <handler>)
(defun depict-catch (markup-stream world level body-annotated-expr arg-binding-expr handler-annotated-expr)
  (depict-statement (markup-stream 'try nil)
    (depict-logical-block (markup-stream 4)
      (depict-break markup-stream)
      (depict-annotated-value-expr markup-stream world body-annotated-expr %stmt%))
    (depict-break markup-stream)
    (depict-semantic-keyword markup-stream 'catch)
    (depict-space markup-stream)
    (depict markup-stream "(")
    (depict-local-variable markup-stream (first arg-binding-expr))
    (depict markup-stream ": ")
    (depict-type-expr markup-stream world *semantic-exception-type-name*)
    (depict markup-stream ")")
    (depict-logical-block (markup-stream 4)
      (depict-break markup-stream)
      (depict-annotated-value-expr markup-stream world handler-annotated-expr %stmt%))))


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


#|
(defun depict-special-function (markup-stream world name-str &rest arg-annotated-exprs)
  (depict-link (markup-stream :external "V-" name-str nil)
    (depict-char-style (markup-stream :global-variable)
      (depict markup-stream name-str)))
  (depict-call-parameters markup-stream world arg-annotated-exprs))
|#


; (empty <vector-expr>)
(defun depict-empty (markup-stream world level vector-annotated-expr)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-logical-block (markup-stream 0)
      (depict-length markup-stream world %term% vector-annotated-expr)
      (depict markup-stream " = ")
      (depict-constant markup-stream 0))))


; (length <vector-expr>)
(defun depict-length (markup-stream world level vector-annotated-expr)
  (declare (ignore level))
  (depict markup-stream "|")
  (depict-annotated-value-expr markup-stream world vector-annotated-expr)
  (depict markup-stream "|"))


; (nth <vector-expr> <n-expr>)
(defun depict-nth (markup-stream world level vector-annotated-expr n-annotated-expr)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-annotated-value-expr markup-stream world vector-annotated-expr %suffix%)
    (depict markup-stream "[")
    (depict-annotated-value-expr markup-stream world n-annotated-expr)
    (depict markup-stream "]")))


; (subseq <vector-expr> <low-expr> [<high-expr>])
(defun depict-subseq (markup-stream world level vector-annotated-expr low-annotated-expr high-annotated-expr)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-annotated-value-expr markup-stream world vector-annotated-expr %suffix%)
    (depict-logical-block (markup-stream 4)
      (depict markup-stream "[")
      (depict-annotated-value-expr markup-stream world low-annotated-expr)
      (depict markup-stream " ...")
      (when high-annotated-expr
        (depict-break markup-stream 1)
        (depict-annotated-value-expr markup-stream world high-annotated-expr))
      (depict markup-stream "]"))))


; (set-nth <vector-expr> <n-expr> <value-expr>)
(defun depict-set-nth (markup-stream world level vector-annotated-expr n-annotated-expr value-annotated-expr)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-annotated-value-expr markup-stream world vector-annotated-expr %suffix%)
    (depict-logical-block (markup-stream 4)
      (depict markup-stream "[")
      (depict-annotated-value-expr markup-stream world n-annotated-expr)
      (depict markup-stream " " :vector-assign-10)
      (depict-break markup-stream 1)
      (depict-annotated-value-expr markup-stream world value-annotated-expr)
      (depict markup-stream "]"))))


; (append <vector-expr> <vector-expr>)
(defun depict-append (markup-stream world level vector1-annotated-expr vector2-annotated-expr)
  (depict-expr-parentheses (markup-stream level %term%)
    (depict-logical-block (markup-stream 0)
      (depict-annotated-value-expr markup-stream world vector1-annotated-expr %term%)
      (depict markup-stream " " :vector-append)
      (depict-break markup-stream 1)
      (depict-annotated-value-expr markup-stream world vector2-annotated-expr %term%))))


;;; Sets

; (set-of-ranges <element-type> <low-expr> <high-expr> ... <low-expr> <high-expr>)
(defun depict-set-of-ranges (markup-stream world level element-type-expr &rest element-annotated-exprs)
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
                       (depict-annotated-value-expr markup-stream world element-annotated-expr1)
                       (when element-annotated-expr2
                         (depict markup-stream " ...")
                         (depict-break markup-stream 1)
                         (depict-annotated-value-expr markup-stream world element-annotated-expr2))))
                 (combine-exprs element-annotated-exprs)
                 :indent 1
                 :prefix "{"
                 :suffix "}"
                 :separator ","
                 :break 1
                 :empty nil)
    (when (null element-annotated-exprs)
      (depict-subscript-type-expr markup-stream world element-type-expr))))


;;; Oneofs

; (oneof <tag> <value-expr> [type])
; [type] was added by scan-oneof-form.
(defun depict-oneof-form (markup-stream world level tag value-annotated-expr type)
  (depict-expr-parentheses (markup-stream level %prefix%)
    (depict-field-name markup-stream tag :reference type)
    (when value-annotated-expr
      (depict-logical-block (markup-stream 4)
        (depict-break markup-stream 1)
        (depict-annotated-value-expr markup-stream world value-annotated-expr %unary%)))))


; (typed-oneof <type-expr> <tag> <value-expr> [type])
(defun depict-typed-oneof (markup-stream world level type-expr tag value-annotated-expr type)
  (depict-expr-parentheses (markup-stream level %prefix%)
    (depict-field-name markup-stream tag :reference type)
    (depict-subscript-type-expr markup-stream world type-expr)
    (when value-annotated-expr
      (depict-logical-block (markup-stream 4)
        (depict-break markup-stream 1)
        (depict-annotated-value-expr markup-stream world value-annotated-expr %unary%)))))


; (case <oneof-expr> [oneof-expr-type] (<tag-spec> <value-expr>) (<tag-spec> <value-expr>) ... (<tag-spec> <value-expr>))
; where each <tag-spec> is either ((<tag> <tag> ... <tag>) nil nil) or ((<tag>) <var> <type>)
(defun depict-case (markup-stream world level oneof-annotated-expr oneof-expr-type &rest annotated-cases)
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
                                 #'(lambda (markup-stream field-name)
                                     (depict-field-name markup-stream field-name :reference oneof-expr-type))
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
                    (depict-annotated-value-expr markup-stream world value-annotated-expr %stmt%)
                    (when (cdr annotated-cases)
                      (depict markup-stream ";")))))
            annotated-cases)
      (depict-break markup-stream)
      (depict-semantic-keyword markup-stream 'end))))


; (select <tag> <oneof-expr> [oneof-expr-type])
; (& <tag> <tuple-expr> [tuple-expr-type])
(defun depict-select-or-& (markup-stream world level tag annotated-expr expr-type)
  (depict-expr-parentheses (markup-stream level %suffix%)
    (depict-annotated-value-expr markup-stream world annotated-expr %suffix%)
    (depict markup-stream ".")
    (depict-field-name markup-stream tag :reference expr-type)))


; (is <tag> <oneof-expr> [oneof-expr-type])
(defun depict-is (markup-stream world level tag oneof-annotated-expr oneof-expr-type)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-annotated-value-expr markup-stream world oneof-annotated-expr %suffix%)
    (depict-space markup-stream)
    (depict-semantic-keyword markup-stream 'is)
    (depict-space markup-stream)
    (depict-field-name markup-stream tag :reference oneof-expr-type)))


;;; Tuples

; (tuple <tuple-type> [type] <field-expr1> ... <field-exprn>)
(defun depict-tuple-form (markup-stream world level type-expr type &rest annotated-exprs)
  (declare (ignore level type-expr))
  (let ((tags (type-tags type)))
    (assert-true (= (length tags) (length annotated-exprs)))
    (depict-list markup-stream
                 #'(lambda (markup-stream parameter)
                     (let ((tag (pop tags)))
                       (depict-field-name markup-stream tag :reference type)
                       (depict-logical-block (markup-stream 4)
                         (depict-break markup-stream 1)
                         (depict-annotated-value-expr markup-stream world parameter %unary%))))
                 annotated-exprs
                 :indent 4
                 :prefix ':tuple-begin
                 :prefix-break 0
                 :suffix ':tuple-end
                 :separator ","
                 :break 1
                 :empty nil)))


;;; Addresses

; (new <value-expr>)
(defun depict-new (markup-stream world level value-annotated-expr)
  (depict-logical-block (markup-stream 5)
    (depict-semantic-keyword markup-stream 'new)
    (depict-space markup-stream)
    (depict-expr-parentheses (markup-stream level %unparenthesized-new%)
      (depict-annotated-value-expr markup-stream world value-annotated-expr
                                   (if (partial-order-< level %unparenthesized-new%) %expr% %prefix%)))))


; (@ <address-expr>)
(defun depict-@ (markup-stream world level address-annotated-expr)
  (depict-expr-parentheses (markup-stream level %prefix%)
    (depict-logical-block (markup-stream 2)
      (depict markup-stream "@")
      (depict-annotated-value-expr markup-stream world address-annotated-expr %prefix%))))


; (@= <address-expr> <value-expr>)
(defun depict-@= (markup-stream world level address-annotated-expr value-annotated-expr)
  (depict-expr-parentheses (markup-stream level %stmt%)
    (depict-logical-block (markup-stream 0)
      (depict markup-stream "@")
      (depict-annotated-value-expr markup-stream world address-annotated-expr %prefix%)
      (depict markup-stream " :=")
      (depict-logical-block (markup-stream 6)
        (depict-break markup-stream 1)
        (depict-annotated-value-expr markup-stream world value-annotated-expr %stmt%)))))


; (address-equal <address-expr1> <address-expr2>)
(defun depict-address-equal (markup-stream world level address1-annotated-expr address2-annotated-expr)
  (depict-expr-parentheses (markup-stream level %relational%)
    (depict-logical-block (markup-stream 0)
      (depict-annotated-value-expr markup-stream world address1-annotated-expr %term%)
      (depict markup-stream " " :identical-10)
      (depict-break markup-stream 1)
      (depict-annotated-value-expr markup-stream world address2-annotated-expr %term%))))


;;; Macros

(defun depict-let-binding (markup-stream world var type-expr value-annotated-expr)
  (depict-logical-block (markup-stream 4)
    (depict-local-variable markup-stream var)
    (depict markup-stream ": ")
    (depict-type-expr markup-stream world type-expr)
    (depict-break markup-stream 1)
    (depict-logical-block (markup-stream 3)
      (depict markup-stream "= ")
      (depict-annotated-value-expr markup-stream world value-annotated-expr %stmt%))))


(defun depict-let-body (markup-stream world body-annotated-expr)
  (depict-break markup-stream)
  (depict-semantic-keyword markup-stream 'in)
  (depict-space markup-stream)
  (depict-logical-block (markup-stream (if (or (macro-annotated-expr? 'let body-annotated-expr)
                                               (macro-annotated-expr? 'letexc body-annotated-expr))
                                         nil
                                         4))
    (depict-annotated-value-expr markup-stream world body-annotated-expr %stmt%)))


; (let ((<var1> <type1> <expr1> [:unused]) ... (<varn> <typen> <exprn> [:unused])) <body>)  ==>
; ((function ((<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <body>) <expr1> ... <exprn>)
(defun depict-let (markup-stream world level annotated-expansion)
  (assert-true (eq (first annotated-expansion) 'expr-annotation:call))
  (let ((function-annotated-expr (second annotated-expansion))
        (arg-annotated-exprs (cddr annotated-expansion)))
    (assert-true (special-form-annotated-expr? 'function function-annotated-expr))
    (let ((arg-binding-exprs (third function-annotated-expr))
          (body-annotated-expr (fourth function-annotated-expr)))
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
; (case <expr> [expr-type]
;   ((abrupt x exception) (typed-oneof <body-type> abrupt x [body-type]))
;   ((normal <var> <type> [:unused]) <body>)))
(defun depict-letexc (markup-stream world level annotated-expansion)
  (assert-true (special-form-annotated-expr? 'case annotated-expansion))
  (let* ((expr-annotated-expr (third annotated-expansion))
         (abrupt-binding (fifth annotated-expansion))
         (abrupt-tag-spec (first abrupt-binding))
         (normal-binding (sixth annotated-expansion))
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
  `(when (depict-mode ,markup-stream ,depict-env :semantics)
     (depict-paragraph (,markup-stream ,paragraph-style)
       ,@body)))


; (%highlight <highlight> <command> ... <command>)
; Depict the commands highlighted with the <highlight> block style.
(defun depict-%highlight (markup-stream world depict-env highlight &rest commands)
  (when commands
    (depict-block-style (markup-stream highlight t)
      (depict-commands markup-stream world depict-env commands))))


; (%section "section-name")
(defun depict-%section (markup-stream world depict-env section-name)
  (declare (ignore world))
  (assert-type section-name string)
  (when (depict-mode markup-stream depict-env nil)
    (depict-paragraph (markup-stream ':section-heading)
      (depict markup-stream section-name))))


; (%subsection "subsection-name")
(defun depict-%subsection (markup-stream world depict-env section-name)
  (declare (ignore world))
  (assert-type section-name string)
  (when (depict-mode markup-stream depict-env nil)
    (depict-paragraph (markup-stream ':subsection-heading)
      (depict markup-stream section-name))))


; (%text <mode> . <styled-text>)
; <mode> is one of:
;   :syntax     This is a comment about the syntax
;   :semantics  This is a comment about the semantics (not displayed when semantics are not displayed)
;   nil         This is a general comment
(defun depict-%text (markup-stream world depict-env mode &rest text)
  (when (depict-mode markup-stream depict-env mode)
    (depict-paragraph (markup-stream ':body-text)
      (let ((grammar-info (depict-env-grammar-info depict-env))
            (*styled-text-world* world))
        (if grammar-info
          (let ((*styled-text-grammar-parametrization* (grammar-info-grammar grammar-info)))
            (depict-styled-text markup-stream text))
          (depict-styled-text markup-stream text))))))


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
            (depict-charclass-action world depict-env (car action-cons) (cdr action-cons) nonterminal)))
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
      (depict-type-name markup-stream name :definition)
      (depict-break markup-stream 1)
      (depict-logical-block (markup-stream 3)
        (depict markup-stream "= ")
        (depict-type-expr markup-stream world type-expr)))))


; (define <name> <type> <value> <destructured>)
; <destructured> is a flag that is true if this define was originally in the form
;   (define (<name> (<arg1> <type1>) ... (<argn> <typen>)) <result-type> <value>)
; and converted into
;   (define <name> (-> (<type1> ... <typen>) <result-type>)
;      (function ((<arg1> <type1>) ... (<argn> <typen>)) <value>)
;      t)
(defun depict-define (markup-stream world depict-env name type-expr value-expr destructured)
  (depict-semantics (markup-stream depict-env)
    (depict-logical-block (markup-stream 2)
      (depict-global-variable markup-stream name :definition)
      (flet
        ((depict-type-and-value (markup-stream type-expr annotated-value-expr)
           (depict-logical-block (markup-stream 0)
             (depict-break markup-stream 1)
             (depict markup-stream ": ")
             (depict-type-expr markup-stream world type-expr))
           (depict-break markup-stream 1)
           (depict-logical-block (markup-stream 3)
             (depict markup-stream "= ")
             (depict-annotated-value-expr markup-stream world annotated-value-expr %max%))))
        
        (let ((annotated-value-expr (nth-value 2 (scan-value world *null-type-env* value-expr))))
          (if destructured
            (progn
              (assert-true (eq (first type-expr) '->))
              (assert-true (special-form-annotated-expr? 'function annotated-value-expr))
              (depict-function-bindings markup-stream world (third annotated-value-expr))
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
                      (eq nonterminal *start-nonterminal*)
                      (hidden-nonterminal? nonterminal))
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
  (depict-general-grammar-symbol markup-stream general-grammar-symbol :reference)
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
    (unless (or (and (general-nonterminal? general-grammar-symbol) (hidden-nonterminal? general-grammar-symbol))
                (grammar-info-charclass-or-partition grammar-info general-grammar-symbol))
      (depict-delayed-action (markup-stream depict-env)
        (depict-semantics (markup-stream depict-env)
          (depict-logical-block (markup-stream 4)
            (depict-declare-action-contents markup-stream world action-name general-grammar-symbol type-expr)))))))


; Declare and define the lexer-action on the charclass given by nonterminal.
(defun depict-charclass-action (world depict-env action-name lexer-action nonterminal)
  (unless (default-action? action-name)
    (depict-delayed-action (markup-stream depict-env)
      (depict-semantics (markup-stream depict-env)
        (depict-logical-block (markup-stream 4)
          (depict-declare-action-contents markup-stream world action-name
                                          nonterminal (lexer-action-type-expr lexer-action))
          (depict-break markup-stream 1)
          (depict-logical-block (markup-stream 3)
            (depict markup-stream "= ")
            (depict-global-variable markup-stream (lexer-action-function-name lexer-action) :external)
            (depict markup-stream "(")
            (depict-general-nonterminal markup-stream nonterminal :reference)
            (depict markup-stream ")")))))))


; (action <action-name> <production-name> <body>)
; <destructured> is a flag that is true if this define was originally in the form
;   (action (<action-name> (<arg1> <type1>) ... (<argn> <typen>)) <production-name> <body>)
; and converted into
;   (action <action-name> <production-name> (function ((<arg1> <type1>) ... (<argn> <typen>)) <body>) t)
(defun depict-action (markup-stream world depict-env action-name production-name body-expr destructured)
  (declare (ignore markup-stream))
  (let* ((grammar-info (checked-depict-env-grammar-info depict-env))
         (grammar (grammar-info-grammar grammar-info))
         (general-production (grammar-general-production grammar production-name))
         (lhs (general-production-lhs general-production)))
    (unless (or (grammar-info-charclass grammar-info lhs)
                (hidden-nonterminal? lhs))
      (depict-delayed-action (markup-stream depict-env)
        (depict-semantics (markup-stream depict-env :semantics-next)
          (depict-logical-block (markup-stream 2)
            (let* ((initial-env (general-production-action-env grammar general-production))
                   (body-annotated-expr (nth-value 2 (scan-value world initial-env body-expr)))
                   (action-grammar-symbols (annotated-expr-grammar-symbols body-annotated-expr)))
              (depict-action-name markup-stream action-name)
              (depict markup-stream :action-begin)
              (depict-general-production markup-stream general-production :reference action-grammar-symbols)
              (depict markup-stream :action-end)
              (flet
                ((depict-body (markup-stream body-annotated-expr)
                   (depict-break markup-stream 1)
                   (depict-logical-block (markup-stream 3)
                     (depict markup-stream "= ")
                     (depict-annotated-value-expr markup-stream world body-annotated-expr %stmt%))))
                
                (if destructured
                  (progn
                    (assert-true (special-form-annotated-expr? 'function body-annotated-expr))
                    (depict-logical-block (markup-stream 10)
                      (depict-break markup-stream 0)
                      (depict-function-bindings markup-stream world (third body-annotated-expr)))
                    (depict-body markup-stream (fourth body-annotated-expr)))
                  (depict-body markup-stream body-annotated-expr))))))))))


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


; (:field <name> <type-expression>)
(defun depict-styled-text-field (markup-stream name type-expression)
  (depict-field-name markup-stream name :reference (scan-type *styled-text-world* type-expression)))

(setf (styled-text-depictor :field) #'depict-styled-text-field)


; (:global <name>)
(defun depict-styled-text-global-variable (markup-stream name)
  (depict-global-variable markup-stream name :reference))

(setf (styled-text-depictor :global) #'depict-styled-text-global-variable)


; (:local <name>)
(setf (styled-text-depictor :local) #'depict-local-variable)


; (:constant <value>)
; <value> can be either an integer, a float, a character, or a string.
(setf (styled-text-depictor :constant) #'depict-constant)


; (:action <name>)
(setf (styled-text-depictor :action) #'depict-action-name)

