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
;;; ECMAScript semantic calculus
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;

(defvar *trace-variables* nil)


#+mcl (dolist (indent-spec '((? . 1) (apply . 1) (funcall . 1) (production . 3) (rule . 2) (function . 1) (letexc . 1) (deftype . 1) (tuple . 1) (%text . 1)))
        (pushnew indent-spec ccl:*fred-special-indent-alist* :test #'equal))


; A strict version of and.
(defun and2 (a b)
  (and a b))

; A strict version of or.
(defun or2 (a b)
  (or a b))

; A strict version of xor.
(defun xor2 (a b)
  (or (and a (not b)) (and (not a) b)))


(defun digit-char-36 (char)
  (assert-non-null (digit-char-p char 36)))


;;; ------------------------------------------------------------------------------------------------------
;;; DOUBLE-PRECISION FLOATING-POINT NUMBERS

(deftype double ()
         '(or float (member :+inf :-inf :nan)))

(defun double? (n)
  (or (floatp n)
      (member n '(:+inf :-inf :nan))))

; Evaluate expr.  If it evaluates successfully, return its values.
; If not, evaluate sign; if it returns a positive value, return :+inf;
; otherwise return :-inf.  sign should not return zero.
(defmacro handle-overflow (expr &body sign)
  `(handler-case ,expr
     (floating-point-overflow () (if (minusp (progn ,@sign)) :-inf :+inf))))


(defun rational-to-double (r)
  (handle-overflow (coerce r 'double-float) r))


; Return true if n is +0 or -0 and false otherwise.
(declaim (inline double-is-zero))
(defun double-is-zero (n)
  (and (floatp n) (zerop n)))


; Return true if n is NaN and false otherwise.
(declaim (inline double-is-nan))
(defun double-is-nan (n)
  (eq n :nan))


; Return true if n is :+inf or :-inf and false otherwise.
(declaim (inline double-is-infinite))
(defun double-is-infinite (n)
  (or (eq n :+inf) (eq n :-inf)))


; Return:
;   less if n<m;
;   equal if n=m;
;   greater if n>m;
;   unordered if either n or m is :nan.
(defun double-compare (n m less equal greater unordered)
  (cond
   ((or (double-is-nan n) (double-is-nan m)) unordered)
   ((eql n m) equal)
   ((or (eq n :+inf) (eq m :-inf)) greater)
   ((or (eq m :+inf) (eq n :-inf)) less)
   ((< n m) less)
   ((> n m) greater)
   (t equal)))


; Return
;    1 if n is +0.0, :+inf, or any positive floating-point number;
;   -1 if n is -0.0, :-inf, or any positive floating-point number;
;    0 if n is :nan.
(defun double-sign (n)
  (case n
    (:+inf 1)
    (:-inf -1)
    (:nan 0)
    (t (round (float-sign n)))))


; Return
;   0 if either n or m is :nan;
;   1 if n and m have the same double-sign;
;  -1 if n and m have different double-signs.
(defun double-sign-xor (n m)
  (* (double-sign n) (double-sign m)))


; Return the absolute value of n.
(defun double-abs (n)
  (case n
    ((:+inf :-inf) :+inf)
    (:nan :nan)
    (t (abs n))))


; Return -n.
(defun double-neg (n)
  (case n
    (:+inf :-inf)
    (:-inf :+inf)
    (:nan :nan)
    (t (- n))))


; Return n+m.
(defun double-add (n m)
  (case n
    (:+inf (case m
             (:-inf :nan)
             (:nan :nan)
             (t :+inf)))
    (:-inf (case m
             (:+inf :nan)
             (:nan :nan)
             (t :-inf)))
    (:nan :nan)
    (t (case m
         (:+inf :+inf)
         (:-inf :-inf)
         (:nan :nan)
         (t (handle-overflow (+ n m)
              (let ((n-sign (float-sign n))
                    (m-sign (float-sign m)))
                (assert-true (= n-sign m-sign)) ;If the signs are opposite, we can't overflow.
                n-sign)))))))


; Return n-m.
(defun double-subtract (n m)
  (double-add n (double-neg m)))


; Return n*m.
(defun double-multiply (n m)
  (let ((sign (double-sign-xor n m))
        (n (double-abs n))
        (m (double-abs m)))
    (let ((result (cond
                   ((zerop sign) :nan)
                   ((eq n :+inf) (if (double-is-zero m) :nan :+inf))
                   ((eq m :+inf) (if (double-is-zero n) :nan :+inf))
                   (t (handle-overflow (* n m) 1)))))
      (if (minusp sign)
        (double-neg result)
        result))))


; Return n/m.
(defun double-divide (n m)
  (let ((sign (double-sign-xor n m))
        (n (double-abs n))
        (m (double-abs m)))
    (let ((result (cond
                   ((zerop sign) :nan)
                   ((eq n :+inf) (if (eq m :+inf) :nan :+inf))
                   ((eq m :+inf) 0d0)
                   ((zerop m) (if (zerop n) :nan :+inf))
                   (t (handle-overflow (/ n m) 1)))))
      (if (minusp sign)
        (double-neg result)
        result))))


; Return n%m, using the ECMAScript definition of %.
(defun double-remainder (n m)
  (cond
   ((or (double-is-nan n) (double-is-nan m) (double-is-infinite n) (double-is-zero m)) :nan)
   ((or (double-is-infinite m) (double-is-zero n)) n)
   (t (float (rem (rational n) (rational m))))))


; Return d truncated towards zero into a 32-bit integer.  Overflows wrap around.
(defun double-to-uint32 (d)
  (case d
    ((:+inf :-inf :nan) 0)
    (t (mod (truncate d) #x100000000))))


;;; ------------------------------------------------------------------------------------------------------
;;; SET UTILITIES

(defun integer-set-min (intset)
  (or (intset-min intset)
      (error "min of empty integer-set")))

(defun character-set-min (intset)
  (code-char (or (intset-min intset)
                 (error "min of empty character-set"))))


(defun integer-set-max (intset)
  (or (intset-max intset)
      (error "max of empty integer-set")))

(defun character-set-max (intset)
  (code-char (or (intset-max intset)
                 (error "max of empty character-set"))))


(defun integer-set-member (elt intset)
  (intset-member? intset elt))

(defun character-set-member (elt intset)
  (intset-member? intset (char-code elt)))


;;; ------------------------------------------------------------------------------------------------------
;;; CODE GENERATION

; Return `(progn ,@statements), optimizing where possible.
(defun gen-progn (&rest statements)
  (if (and (= (length statements) 1)
           (let ((first-statement (first statements)))
             (not (and (consp first-statement)
                       (eq (first first-statement) 'declare)))))
    (first statements)
    (cons 'progn statements)))


; Return `(funcall ,function-value ,@arg-values), optimizing where possible.
(defun gen-apply (function-value &rest arg-values)
  (if (and (consp function-value)
           (eq (first function-value) 'function)
           (consp (rest function-value))
           (second function-value)
           (null (cddr function-value)))
    (let ((stripped-function-value (second function-value)))
      (if (and (consp stripped-function-value)
               (eq (first stripped-function-value) 'function)
               (listp (second stripped-function-value))
               (cddr stripped-function-value)
               (every #'(lambda (arg)
                          (and (identifier? arg)
                               (not (eql (first-symbol-char arg) #\&))))
                      (second stripped-function-value)))
        (let ((function-args (second stripped-function-value))
              (function-body (cddr stripped-function-value)))
          (assert-true (= (length function-args) (length arg-values)))
          (if function-args
            (list* 'let
                   (mapcar #'list function-args arg-values)
                   function-body)
            (apply #'gen-progn function-body)))
        (cons stripped-function-value arg-values)))
    (list* 'funcall function-value arg-values)))


; Return `#'(lambda ,args (declare (ignore-if-unused ,@args)) ,body-code), optimizing
; where possible.
(defun gen-lambda (args body-code)
  (if args
    `#'(lambda ,args (declare (ignore-if-unused . ,args)) ,body-code)
    `#'(lambda () ,body-code)))


; If expr is a lambda-expression, return an equivalent expression that has
; the given name (which may be a symbol or a string; if it's a string, it is interned
; in the given package).  Otherwise, return expr unchanged.
; Attaching a name to lambda-expressions helps in debugging code by identifying
; functions in debugger backtraces.
(defun name-lambda (expr name &optional package)
  (if (and (consp expr)
           (eq (first expr) 'function)
           (consp (rest expr))
           (consp (second expr))
           (eq (first (second expr)) 'lambda)
           (null (cddr expr)))
    (let ((name (if (symbolp name)
                  name
                  (intern name package))))
      ;Avoid trouble when name is a lisp special form like if or lambda.
      (when (special-form-p name)
        (setq name (gensym name)))
      `(flet ((,name ,@(rest (second expr))))
         #',name))
    expr))


; Intern n symbols in the current package with names <prefix>0, <prefix>1, ...,
; <prefix>n-1, where <prefix> is the value of the prefix string.
; Return a list of these n symbols concatenated to the front of rest.
(defun intern-n-vars-with-prefix (prefix n rest)
  (if (zerop n)
    rest
    (intern-n-vars-with-prefix prefix (1- n) (cons (intern (format nil "~A~D" prefix n)) rest))))


;;; ------------------------------------------------------------------------------------------------------
;;; LF TOKENS

;;; Each symbol in the LF package is a variant of a terminal that represents that terminal preceded by one
;;; or more line breaks.

(defvar *lf-package* (make-package "LF" :use nil))

(defun make-lf-terminal (terminal)
  (assert-true (not (lf-terminal? terminal)))
  (multiple-value-bind (lf-terminal present) (intern (symbol-name terminal) *lf-package*)
    (unless (eq present :external)
      (export lf-terminal *lf-package*)
      (setf (get lf-terminal :sort-key) (concatenate 'string (symbol-name terminal) " "))
      (setf (get lf-terminal :origin) terminal)
      (setf (get terminal :lf-terminal) lf-terminal))
    lf-terminal))

(defun lf-terminal? (terminal)
  (eq (symbol-package terminal) *lf-package*))


(declaim (inline terminal-lf-terminal lf-terminal-terminal))
(defun terminal-lf-terminal (terminal)
  (get terminal :lf-terminal))
(defun lf-terminal-terminal (lf-terminal)
  (get lf-terminal :origin))


; Ensure that for each transition on a LF: terminal in the grammar there exists a corresponding transition
; on a non-LF: terminal.
(defun ensure-lf-subset (grammar)
  (all-state-transitions
   #'(lambda (state transitions-hash)
       (dolist (transition-pair (state-transitions state))
         (let ((terminal (car transition-pair)))
           (when (lf-terminal? terminal)
             (unless (equal (cdr transition-pair) (gethash (lf-terminal-terminal terminal) transitions-hash))
               (format *error-output* "State ~S: transition on ~S differs from transition on ~S~%"
                       state terminal (lf-terminal-terminal terminal)))))))
   grammar))


; Print a list of transitions on non-LF: terminals that do not have corresponding LF: transitions.
; Return a list of non-LF: terminals which behave identically to the corresponding LF: terminals.
(defun show-non-lf-only-transitions (grammar)
  (let ((invariant-terminalset (make-full-terminalset grammar))
        (terminals-vector (grammar-terminals grammar)))
    (dotimes (n (length terminals-vector))
      (let ((terminal (svref terminals-vector n)))
        (when (lf-terminal? terminal)
          (terminalset-difference-f invariant-terminalset (make-terminalset grammar terminal)))))
    (all-state-transitions
     #'(lambda (state transitions-hash)
         (dolist (transition-pair (state-transitions state))
           (let ((terminal (car transition-pair)))
             (unless (lf-terminal? terminal)
               (let ((lf-terminal (terminal-lf-terminal terminal)))
                 (when lf-terminal
                   (let ((lf-terminal-transition (gethash lf-terminal transitions-hash)))
                     (cond
                      ((null lf-terminal-transition)
                       (terminalset-difference-f invariant-terminalset (make-terminalset grammar terminal))
                       (format *error-output* "State ~S has transition on ~S but not on ~S~%"
                               state terminal lf-terminal))
                      ((not (equal (cdr transition-pair) lf-terminal-transition))
                       (terminalset-difference-f invariant-terminalset (make-terminalset grammar terminal))
                       (format *error-output* "State ~S transition on ~S differs from transition on ~S~%"
                               state terminal lf-terminal))))))))))
     grammar)
    (terminalset-list grammar invariant-terminalset)))


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR-INFO

(defstruct (grammar-info (:constructor make-grammar-info (name grammar &optional lexer))
                         (:copier nil)
                         (:predicate grammar-info?))
  (name nil :type symbol :read-only t)               ;The name of this grammar
  (grammar nil :type grammar :read-only t)           ;This grammar
  (lexer nil :type (or null lexer) :read-only t))    ;This grammar's lexer if this is a lexer grammar; nil if not


; Return the charclass that defines the given lexer nonterminal or nil if none.
(defun grammar-info-charclass (grammar-info nonterminal)
  (let ((lexer (grammar-info-lexer grammar-info)))
    (and lexer (lexer-charclass lexer nonterminal))))


; Return the charclass or partition that defines the given lexer nonterminal or nil if none.
(defun grammar-info-charclass-or-partition (grammar-info nonterminal)
  (let ((lexer (grammar-info-lexer grammar-info)))
    (and lexer (or (lexer-charclass lexer nonterminal)
                   (gethash nonterminal (lexer-partitions lexer))))))


;;; ------------------------------------------------------------------------------------------------------
;;; WORLDS

(defstruct (world (:constructor allocate-world)
                  (:copier nil)
                  (:predicate world?))
  (conditionals nil :type list)                      ;Assoc list of (conditional . highlight), where highlight can be a style keyword, nil (no style), or 'delete
  (package nil :type package)                        ;The package in which this world's identifiers are interned
  (n-type-names 0 :type integer)                     ;Number of type names defined so far
  (types-reverse nil :type (or null hash-table))     ;Hash table of (kind tags parameters) -> type; nil if invalid
  (oneof-tags nil :type (or null hash-table))        ;Hash table of (oneof-tag . field-type) -> (must-be-unique oneof-type ... oneof-type); nil if invalid
  (bottom-type nil :type (or null type))             ;Subtype of all types used for nonterminating computations
  (void-type nil :type (or null type))               ;Type used for placeholders
  (boolean-type nil :type (or null type))            ;Type used for booleans
  (integer-type nil :type (or null type))            ;Type used for integers
  (rational-type nil :type (or null type))           ;Type used for rational numbers
  (double-type nil :type (or null type))             ;Type used for double-precision floating-point numbers
  (character-type nil :type (or null type))          ;Type used for characters
  (string-type nil :type (or null type))             ;Type used for strings (vectors of characters)
  (grammar-infos nil :type list)                     ;List of grammar-info
  (commands-source nil :type list))                  ;List of source code of all commands applied to this world


; Return the name of the world.
(defun world-name (world)
  (package-name (world-package world)))


; Return a symbol in the given package whose value is that package's world structure.
(defun world-access-symbol (package)
  (find-symbol "*WORLD*" package))


; Return the world that created the given package.
(declaim (inline package-world))
(defun package-world (package)
  (symbol-value (world-access-symbol package)))


; Return the world that contains the given symbol.
(defun symbol-world (symbol)
  (package-world (symbol-package symbol)))


; Delete the world and its package.
(defun delete-world (world)
  (let ((package (world-package world)))
    (when package
      (delete-package package)))
  (setf (world-package world) nil))


; Create a world using a package with the given name.
; If the package is already used for another world, its contents
; are erased and the other world deleted.
(defun make-world (name)
  (assert-type name string)
  (let ((p (find-package name)))
    (when p
      (let* ((access-symbol (world-access-symbol p))
             (p-world (and (boundp access-symbol) (symbol-value access-symbol))))
        (unless p-world
          (error "Package ~A already in use" name))
        (assert-true (eq (world-package p-world) p))
        (delete-world p-world))))
  (let* ((p (make-package name :use nil))
         (world (allocate-world
                 :package p
                 :types-reverse (make-hash-table :test #'equal)
                 :oneof-tags (make-hash-table :test #'equal)))
         (access-symbol (intern "*WORLD*" p)))
    (set access-symbol world)
    (export access-symbol p)
    world))


; Intern s (which should be a symbol or a string) in this world's
; package and return the resulting symbol.
(defun world-intern (world s)
  (intern (string s) (world-package world)))


; Export symbol in its package, which must belong to some world.
(defun export-symbol (symbol)
  (assert-true (symbol-in-any-world symbol))
  (export symbol (symbol-package symbol)))


; Call f on each external symbol defined in the world's package.
(declaim (inline each-world-external-symbol))
(defun each-world-external-symbol (world f)
  (each-package-external-symbol (world-package world) f))


; Call f on each external symbol defined in the world's package that has
; a property with the given name.
; f takes two arguments:
;   the symbol
;   the value of the property
(defun each-world-external-symbol-with-property (world property f)
  (each-world-external-symbol
   world
   #'(lambda (symbol)
       (let ((value (get symbol property *get2-nonce*)))
         (unless (eq value *get2-nonce*)
           (funcall f symbol value))))))


; Return a list of all external symbols defined in the world's package that have
; a property with the given name.
; The list is sorted by symbol names.
(defun all-world-external-symbols-with-property (world property)
  (let ((list nil))
    (each-world-external-symbol
     world
     #'(lambda (symbol)
         (let ((value (get symbol property *get2-nonce*)))
           (unless (eq value *get2-nonce*)
             (push symbol list)))))
    (sort list #'string<)))


; Return true if s is a symbol that is defined in this world's package.
(declaim (inline symbol-in-world))
(defun symbol-in-world (world s)
  (and (symbolp s) (eq (symbol-package s) (world-package world))))


; Return true if s is a symbol that is defined in any world's package.
(defun symbol-in-any-world (s)
  (and (symbolp s)
       (let* ((package (symbol-package s))
              (access-symbol (world-access-symbol package)))
         (and (boundp access-symbol) (typep (symbol-value access-symbol) 'world)))))


; Return a list of grammars in the world
(defun world-grammars (world)
  (mapcar #'grammar-info-grammar (world-grammar-infos world)))


; Return the grammar-info with the given name in the world
(defun world-grammar-info (world name)
  (find name (world-grammar-infos world) :key #'grammar-info-name))


; Return the grammar with the given name in the world
(defun world-grammar (world name)
  (let ((grammar-info (world-grammar-info world name)))
    (and grammar-info (grammar-info-grammar grammar-info))))


; Return the lexer with the given name in the world
(defun world-lexer (world name)
  (let ((grammar-info (world-grammar-info world name)))
    (and grammar-info (grammar-info-lexer grammar-info))))


; Return a list of highlights allowed in this world.
(defun world-highlights (world)
  (let ((highlights nil))
    (dolist (c (world-conditionals world))
      (let ((highlight (cdr c)))
        (unless (or (null highlight) (eq highlight 'delete))
          (pushnew highlight highlights))))
    (nreverse highlights)))


; Return the highlight to which the given conditional maps.
; Return 'delete if the conditional should be omitted.
(defun resolve-conditional (world conditional)
  (let ((h (assoc conditional (world-conditionals world))))
    (if h
      (cdr h)
      (error "Bad conditional ~S" conditional))))


;;; ------------------------------------------------------------------------------------------------------
;;; SYMBOLS

;;; The following properties are attached to exported symbols in the world:
;;;
;;;   :preprocess    preprocessor function ((preprocessor-state id . form-arg-list) -> form-list re-preprocess) if this identifier
;;;                  is a preprocessor command like 'grammar, 'lexer, or 'production
;;;
;;;   :command       expression code generation function ((world grammar-info-var . form-arg-list) -> void) if this identifier
;;;                  is a command like 'deftype or 'define
;;;   :special-form  expression code generation function ((world type-env id . form-arg-list) -> code, type, annotated-expr)
;;;                  if this identifier is a special form like 'if or 'function
;;;
;;;   :primitive     primitive structure if this identifier is a primitive
;;;
;;;   :macro         lisp expansion function ((world type-env . form-arg-list) -> expansion) if this identifier is a macro
;;;
;;;   :type-constructor  expression code generation function ((world allow-forward-references . form-arg-list) -> type) if this
;;;                  identifier is a type constructor like '->, 'vector, 'set, 'tuple, 'oneof, or 'address
;;;   :deftype       type if this identifier is a type; nil if this identifier is a forward-referenced type
;;;
;;;   <value>        value of this identifier if it is a variable
;;;   :value-code    lisp code that was evaluated to produce <value>
;;;   :value-expr    unparsed expression defining the value of this identifier if it is a variable
;;;   :type          type of this identifier if it is a variable
;;;   :type-expr     unparsed expression defining the type of this identifier if it is a variable
;;;
;;;   :action        list of (grammar-info . grammar-symbol) that declare this action if this identifier is an action name
;;;
;;;   :depict-command           depictor function ((markup-stream world depict-env . form-arg-list) -> void)
;;;   :depict-type-constructor  depictor function ((markup-stream world level . form-arg-list) -> void)
;;;   :depict-special-form      depictor function ((markup-stream world level . form-annotated-arg-list) -> void)
;;;   :depict-macro             depictor function ((markup-stream world level . form-annotated-arg-list) -> void)
;;;


; Return the code of the value associated with the given symbol or default if none.
; This macro is appropriate for use with setf.
(defmacro symbol-code (symbol &optional default)
  `(get ,symbol :code ,@(and default (list default))))


; Return the preprocessor action associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-preprocessor-function (symbol)
  `(get ,symbol :preprocess))


; Return the macro definition associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-macro (symbol)
  `(get ,symbol :macro))


; Return the primitive definition associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-primitive (symbol)
  `(get ,symbol :primitive))


; Return the type definition associated with the given symbol.
; Return nil if the symbol is a forward-referenced type.
; If the symbol has no type definition at all, return default
; (or nil if not specified).
; This macro is appropriate for use with setf.
(defmacro symbol-type-definition (symbol &optional default)
  `(get ,symbol :deftype ,@(and default (list default))))


; Return true if this symbol's symbol-type-definition is user-defined.
; This macro is appropriate for use with setf.
(defmacro symbol-type-user-defined (symbol)
  `(get ,symbol 'type-user-defined))


; Call f on each type definition, including forward-referenced types, in the world.
; f takes two arguments:
;   the symbol
;   the type (nil if forward-referenced)
(defun each-type-definition (world f)
  (each-world-external-symbol-with-property world :deftype f))


; Return a sorted list of the names of all type definitions, including
; forward-referenced types, in the world.
(defun world-type-definitions (world)
  (all-world-external-symbols-with-property world :deftype))


; Return the type of the variable associated with the given symbol or nil if none.
; This macro is appropriate for use with setf.
(defmacro symbol-type (symbol)
  `(get ,symbol :type))


; Return true if there is a variable associated with the given symbol.
(declaim (inline symbol-has-variable))
(defun symbol-has-variable (symbol)
  (not (eq (get symbol *get2-nonce*) *get2-nonce*)))


; Return a list of (grammar-info . grammar-symbol) pairs that each indicate
; a grammar and a grammar-symbol in that grammar that has an action named by the given symbol.
; This macro is appropriate for use with setf.
(defmacro symbol-action (symbol)
  `(get ,symbol :action))


;;; ------------------------------------------------------------------------------------------------------
;;; TYPES

(deftype typekind ()
         '(member     ;tags             ;parameters
           :bottom    ;nil              ;nil
           :void      ;nil              ;nil
           :boolean   ;nil              ;nil
           :integer   ;nil              ;nil
           :rational  ;nil              ;nil
           :double    ;nil              ;nil
           :character ;nil              ;nil
           :->        ;nil              ;(result-type arg1-type arg2-type ... argn-type)
           :vector    ;nil              ;(element-type)
           :set       ;nil              ;(element-type)
           :tuple     ;(tag1 ... tagn)  ;(element1-type ... elementn-type)
           :oneof     ;(tag1 ... tagn)  ;(element1-type ... elementn-type)
           :address)) ;nil              ;(element-type)


; Return true if typekind1 is the same or more specific (i.e. a subtype) than typekind2.
(defun typekind<= (typekind1 typekind2)
  (or (eq typekind1 typekind2)
      (eq typekind1 :bottom)
      (and (eq typekind1 :integer) (eq typekind2 :rational))))


(defstruct (type (:constructor allocate-type (kind tags parameters))
                 (:predicate type?))
  (name nil :type symbol)                          ;This type's name; nil if this type is anonymous
  (name-serial-number nil :type (or null integer)) ;This type's name's serial number; nil if this type is anonymous
  (kind nil :type typekind :read-only t)           ;This type's kind
  (tags nil :type list :read-only t)               ;List of tuple or oneof tags
  (parameters nil :type list :read-only t))        ;List of parameter types (either types or symbols if forward-referenced) describing a compound type


(declaim (inline make-->-type))
(defun make-->-type (world argument-types result-type)
  (make-type world :-> nil (cons result-type argument-types)))

(declaim (inline ->-argument-types))
(defun ->-argument-types (type)
  (assert-true (eq (type-kind type) :->))
  (cdr (type-parameters type)))

(declaim (inline ->-result-type))
(defun ->-result-type (type)
  (assert-true (eq (type-kind type) :->))
  (car (type-parameters type)))


(declaim (inline make-vector-type))
(defun make-vector-type (world element-type)
  (make-type world :vector nil (list element-type)))

(declaim (inline vector-element-type))
(defun vector-element-type (type)
  (assert-true (eq (type-kind type) :vector))
  (car (type-parameters type)))


(declaim (inline make-set-type))
(defun make-set-type (world element-type)
  (make-type world :set nil (list element-type)))

(declaim (inline set-element-type))
(defun set-element-type (type)
  (assert-true (eq (type-kind type) :set))
  (car (type-parameters type)))


; Return the type of the oneof's or tuple's field corresponding to the given tag
; or nil if the tag is not present in the oneof's or tuple's tags.
(defun field-type (type tag)
  (assert-true (member (type-kind type) '(:oneof :tuple)))
  (let ((pos (position tag (type-tags type))))
    (and pos (nth pos (type-parameters type)))))


(declaim (inline make-address-type))
(defun make-address-type (world element-type)
  (make-type world :address nil (list element-type)))

(declaim (inline address-element-type))
(defun address-element-type (type)
  (assert-true (eq (type-kind type) :address))
  (car (type-parameters type)))


; Return true if type1 is the same or more specific (i.e. a subtype) than type2.
(defun type<= (type1 type2)
  (or (eq type1 type2)
      (let ((kind1 (type-kind type1))
            (kind2 (type-kind type2)))
        (or (eq kind1 :bottom)
            (and (eq kind1 :integer) (eq kind2 :rational))
            (and (eq kind1 :->) (eq kind2 :->)
                 ; For now we require the argument types to match exactly.
                 (equal (->-argument-types type1) (->-argument-types type2))
                 ; This might fall into an infinite loop, but it's OK for now.
                 (type<= (->-result-type type1) (->-result-type type2)))))))


; Return the most specific common supertype of type1 and type2 or nil if there is none.
(defun type-lub (type1 type2)
  (cond
   ((type<= type1 type2) type2)
   ((type<= type2 type1) type1)
   (t nil)))


; Return true if serial-number-1 is less than serial-number-2.
; Each serial-number is either an integer or nil, which is considered to
; be positive infinity.
(defun serial-number-< (serial-number-1 serial-number-2)
  (and serial-number-1
       (or (null serial-number-2)
           (< serial-number-1 serial-number-2))))


; Print the type nicely on the given stream.  If expand1 is true then print
; the type's top level even if it has a name.  In all other cases expand
; anonymous types but abbreviate named types by their names.
(defun print-type (type &optional (stream t) expand1)
  (if (and (type-name type) (not expand1))
    (write-string (symbol-name (type-name type)) stream)
    (labels
      ((print-tuple-or-oneof (kind-string)
         (pprint-logical-block (stream (mapcar #'cons (type-tags type) (type-parameters type))
                                       :prefix "(" :suffix ")")
           (write-string kind-string stream)
           (pprint-exit-if-list-exhausted)
           (format stream " ~@_")
           (pprint-indent :current 0 stream)
           (loop
             (let ((tag-and-type (pprint-pop)))
               (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                 (write (car tag-and-type) :stream stream)
                 (format stream " ~@_")
                 (print-type (cdr tag-and-type) stream))
               (pprint-exit-if-list-exhausted)
               (format stream " ~:_")))
           (format stream " ~_")
           (print-type (->-result-type type) stream))))
      
      (case (type-kind type)
        (:bottom (write-string "bottom" stream))
        (:void (write-string "void" stream))
        (:boolean (write-string "boolean" stream))
        (:integer (write-string "integer" stream))
        (:rational (write-string "rational" stream))
        (:double (write-string "double" stream))
        (:character (write-string "character" stream))
        (:-> (pprint-logical-block (stream nil :prefix "(" :suffix ")")
               (format stream "-> ~@_")
               (pprint-indent :current 0 stream)
               (pprint-logical-block (stream (->-argument-types type) :prefix "(" :suffix ")")
                 (pprint-exit-if-list-exhausted)
                 (loop
                   (print-type (pprint-pop) stream)
                   (pprint-exit-if-list-exhausted)
                   (format stream " ~:_")))
               (format stream " ~_")
               (print-type (->-result-type type) stream)))
        (:vector (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                   (format stream "vector ~@_")
                   (print-type (vector-element-type type) stream)))
        (:set (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                (format stream "set ~@_")
                (print-type (set-element-type type) stream)))
        (:tuple (print-tuple-or-oneof "tuple"))
        (:oneof (print-tuple-or-oneof "oneof"))
        (:address (pprint-logical-block (stream nil :prefix "(" :suffix ")")
                    (format stream "address ~@_")
                    (print-type (address-element-type type) stream)))
        (t (error "Bad typekind ~S" (type-kind type)))))))


; Same as print-type except that accumulates the output in a string
; and returns that string.
(defun print-type-to-string (type &optional expand1)
  (with-output-to-string (stream)
    (print-type type stream expand1)))


(defmethod print-object ((type type) stream)
  (print-unreadable-object (type stream)
    (format stream "type ~@_")
    (let ((name (type-name type)))
      (when name
        (format stream "~A = ~@_" name)))
    (print-type type stream t)))


; Register all of the oneof type's tags in the world's oneof-tags hash table.
; The hash table is indexed by pairs (tag . field-type) and is used to look up a
; oneof type given just a tag and its field's type.  The data in the hash table
; consists of lists (flag oneof-type ... oneof-type).  The flag is true if such a
; lookup has been performed (in which case the data must contain exactly one oneof-type
; and it is an error to add another one).
(defun register-oneof-tags (world oneof-type)
  (let ((oneof-tags-hash (world-oneof-tags world)))
    (mapc #'(lambda (tag field-type)
              (let* ((key (cons tag field-type))
                     (data (gethash key oneof-tags-hash)))
                (cond
                 ((null data)
                  (setf (gethash key oneof-tags-hash) (list nil oneof-type)))
                 ((not (car data))
                  (push oneof-type (cdr data)))
                 (t (error "Ambiguous oneof lookup of tag ~A: ~A.  Possibilities are ~A or ~A"
                           tag
                           (print-type-to-string field-type)
                           (print-type-to-string (second data))
                           (print-type-to-string oneof-type))))))
          (type-tags oneof-type)
          (type-parameters oneof-type))))


; Look up a oneof type given one of its tags and the corresponding field type.
; Signal an error if there is no such type or there is more than one matching type.
(defun lookup-oneof-tag (world tag field-type)
  (let ((data (gethash (cons tag field-type) (world-oneof-tags world))))
    (cond
     ((null data)
      (error "No known oneof type with tag ~A: ~A" tag (print-type-to-string field-type)))
     ((cddr data)
      (error "Ambiguous oneof lookup of tag ~A: ~A.  Possibilities are ~S" tag (print-type-to-string field-type) (cdr data)))
     (t
      (setf (first data) t)
      (second data)))))


; Create or reuse a type with the given kind, tags, and parameters.
; A type is reused if one already exists with equal kind, tags, and parameters.
; Return the type.
(defun make-type (world kind tags parameters)
  (let ((reverse-key (list kind tags parameters)))
    (or (gethash reverse-key (world-types-reverse world))
        (let ((type (allocate-type kind tags parameters)))
          (when (eq kind :oneof)
            (register-oneof-tags world type))
          (setf (gethash reverse-key (world-types-reverse world)) type)))))


; Provide a new symbol for the type.  A type can have zero or more names.
; Signal an error if the name is already used.
; user-defined is true if this is a user-defined type rather than a predefined type.
(defun add-type-name (world type symbol user-defined)
  (assert-true (symbol-in-world world symbol))
  (when (symbol-type-definition symbol)
    (error "Attempt to redefine type ~A" symbol))
  ;If the old type was anonymous, give it this name.
  (unless (type-name type)
    (setf (type-name type) symbol)
    (setf (type-name-serial-number type) (world-n-type-names world)))
  (incf (world-n-type-names world))
  (setf (symbol-type-definition symbol) type)
  (when user-defined
    (setf (symbol-type-user-defined symbol) t))
  (export-symbol symbol))


; Return an existing type with the given symbol, which must be interned in a world's package.
; Signal an error if there isn't an existing type.  If allow-forward-references is true and
; symbol is an undefined type identifier, allow it, create a forward-referenced type, and return symbol.
(defun get-type (symbol &optional allow-forward-references)
  (or (symbol-type-definition symbol)
      (if allow-forward-references
        (progn
          (setf (symbol-type-definition symbol) nil)
          symbol)
        (error "Undefined type ~A" symbol))))


; Scan a type-expr to produce a type.  Return that type.
; If allow-forward-references is true and type-expr is an undefined type identifier,
; allow it, create a forward-referenced type in the world, and return type-expr unchanged.
; If allow-forward-references is true, also allow undefined type
; identifiers deeper within type-expr (anywhere except at its top level).
; If type-expr is already a type, return it unchanged.
(defun scan-type (world type-expr &optional allow-forward-references)
  (cond
   ((identifier? type-expr)
    (get-type (world-intern world type-expr) allow-forward-references))
   ((type? type-expr)
    type-expr)
   (t (let ((type-constructor (and (consp type-expr)
                                   (symbolp (first type-expr))
                                   (get (world-intern world (first type-expr)) :type-constructor))))
        (if type-constructor
          (apply type-constructor world allow-forward-references (rest type-expr))
          (error "Bad type ~S" type-expr))))))


; Same as scan-type except that ensure that the type has the expected kind.
; Return the type.
(defun scan-kinded-type (world type-expr expected-type-kind)
  (let ((type (scan-type world type-expr)))
    (unless (eq (type-kind type) expected-type-kind)
      (error "Expected ~(~A~) but got ~A" expected-type-kind (print-type-to-string type)))
    type))


; (-> (<arg-type1> ... <arg-typen>) <result-type>)
(defun scan--> (world allow-forward-references arg-type-exprs result-type-expr)
  (unless (listp arg-type-exprs)
    (error "Bad -> argument type list ~S" arg-type-exprs))
  (make-->-type world
                (mapcar #'(lambda (te) (scan-type world te allow-forward-references)) arg-type-exprs)
                (scan-type world result-type-expr allow-forward-references)))


; (vector <element-type>)
(defun scan-vector (world allow-forward-references element-type)
  (make-vector-type world (scan-type world element-type allow-forward-references)))


; (set <element-type>)
(defun scan-set (world allow-forward-references element-type)
  (make-set-type world (scan-type world element-type allow-forward-references)))


; (address <element-type>)
(defun scan-address (world allow-forward-references element-type)
  (make-address-type world (scan-type world element-type allow-forward-references)))


(defun scan-tuple-or-oneof (world allow-forward-references kind tag-pairs tags-so-far types-so-far)
  (if tag-pairs
    (let ((tag-pair (car tag-pairs)))
      (when (and (identifier? tag-pair) (eq kind :oneof))
        (setq tag-pair (list tag-pair 'void)))
      (unless (and (consp tag-pair) (identifier? (first tag-pair))
                   (second tag-pair) (null (cddr tag-pair)))
        (error "Bad oneof or tuple pair ~S" tag-pair))
      (let ((tag (first tag-pair)))
        (when (member tag tags-so-far)
          (error "Duplicate oneof or tuple tag ~S" tag))
        (scan-tuple-or-oneof
         world
         allow-forward-references
         kind
         (cdr tag-pairs)
         (cons tag tags-so-far)
         (cons (scan-type world (second tag-pair) allow-forward-references) types-so-far))))
    (make-type world kind (nreverse tags-so-far) (nreverse types-so-far))))

; (oneof (<tag1> <type1>) ... (<tagn> <typen>))
(defun scan-oneof (world allow-forward-references &rest tags-and-types)
  (scan-tuple-or-oneof world allow-forward-references :oneof tags-and-types nil nil))

; (tuple (<tag1> <type1>) ... (<tagn> <typen>))
(defun scan-tuple (world allow-forward-references &rest tags-and-types)
  (scan-tuple-or-oneof world allow-forward-references :tuple tags-and-types nil nil))


; Scan tag to produce a tag that is present in the given tuple or oneof type.
; Return the tag and its field type.
(defun scan-tag (type tag)
  (let ((field-type (field-type type tag)))
    (unless field-type
      (error "Tag ~S not present in ~A" tag (print-type-to-string type)))
    (values tag field-type)))


; Resolve all forward type references to refer to their target types.
; Signal an error if any unresolved type references remain.
; Only types reachable from some type name are affected.  It is the caller's
; responsibility to make sure that these are the only types that exist.
; Return a list of all type structures encountered.
(defun resolve-forward-types (world)
  (setf (world-types-reverse world) nil)
  (setf (world-oneof-tags world) nil)
  (let ((visited-types (make-hash-table :test #'eq)))
    (labels
      ((resolve-in-type (type)
         (unless (gethash type visited-types)
           (setf (gethash type visited-types) t)
           (do ((parameter-types (type-parameters type) (cdr parameter-types)))
               ((endp parameter-types))
             (let ((parameter-type (car parameter-types)))
               (unless (typep parameter-type 'type)
                 (setq parameter-type (get-type parameter-type))
                 (setf (car parameter-types) parameter-type))
               (resolve-in-type parameter-type))))))
      (each-type-definition
       world
       #'(lambda (symbol type)
           (unless type
             (error "Undefined type ~A" symbol))
           (resolve-in-type type))))
    (hash-table-keys visited-types)))


; Recompute the types-reverse and oneof-tags hash tables from the types in the types
; hash table and their constituents.
(defun recompute-type-caches (world)
  (let ((types-reverse (make-hash-table :test #'equal)))
    (setf (world-oneof-tags world) (make-hash-table :test #'equal))
    (labels
      ((visit-type (type)
         (let ((reverse-key (list (type-kind type) (type-tags type) (type-parameters type))))
           (assert-true (eq (gethash reverse-key types-reverse type) type))
           (unless (gethash reverse-key types-reverse)
             (setf (gethash reverse-key types-reverse) type)
             (when (eq (type-kind type) :oneof)
               (register-oneof-tags world type))
             (mapc #'visit-type (type-parameters type))))))
      (each-type-definition
       world
       #'(lambda (symbol type)
           (declare (ignore symbol))
           (visit-type type))))
    (setf (world-types-reverse world) types-reverse)))



; Make all equivalent types be eq.  Only types reachable from some type name
; are affected, and names may be redirected to different type structures than
; the ones to which they currently point.  It is the caller's responsibility
; to make sure that there are no current outstanding references to types other
; than via type names.
;
; This function calls resolve-forward-types before making equivalent types be eq
; and recompute-type-caches just before returning.
;
; This function works by initially assuming that all types with the same kind
; and tags are the same type and then iterately determining which ones must be
; different because they contain different parameter types.
(defun unite-types (world)
  (let* ((types (resolve-forward-types world))
         (n-types (length types)))
    (labels
      ((gen-cliques-1 (get-key)
         (let ((types-to-cliques (make-hash-table :test #'eq :size n-types))
               (keys-to-cliques (make-hash-table :test #'equal))
               (n-cliques 0))
           (dolist (type types)
             (let* ((key (funcall get-key type))
                    (clique (gethash key keys-to-cliques)))
               (unless clique
                 (setq clique n-cliques)
                 (incf n-cliques)
                 (setf (gethash key keys-to-cliques) clique))
               (setf (gethash type types-to-cliques) clique)))
           (values n-cliques types-to-cliques)))
       
       (gen-cliques (n-old-cliques types-to-old-cliques)
         (labels
           ((get-old-clique (type)
              (assert-non-null (gethash type types-to-old-cliques)))
            (get-type-key (type)
              (cons (get-old-clique type)
                    (mapcar #'get-old-clique (type-parameters type)))))
           (multiple-value-bind (n-new-cliques types-to-new-cliques) (gen-cliques-1 #'get-type-key)
             (assert-true (>= n-new-cliques n-old-cliques))
             (if (/= n-new-cliques n-old-cliques)
               (gen-cliques n-new-cliques types-to-new-cliques)
               (translate-types n-new-cliques types-to-new-cliques)))))
       
       (translate-types (n-cliques types-to-cliques)
         (let ((clique-representatives (make-array n-cliques :initial-element nil)))
           (maphash #'(lambda (type clique)
                        (let ((representative (svref clique-representatives clique)))
                          (when (or (null representative)
                                    (serial-number-< (type-name-serial-number type) (type-name-serial-number representative)))
                            (setf (svref clique-representatives clique) type))))
                    types-to-cliques)
           (assert-true (every #'identity clique-representatives))
           (labels
             ((map-type (type)
                (svref clique-representatives (gethash type types-to-cliques))))
             (dolist (type types)
               (do ((parameter-types (type-parameters type) (cdr parameter-types)))
                   ((endp parameter-types))
                 (setf (car parameter-types) (map-type (car parameter-types)))))
             (each-type-definition
              world
              #'(lambda (symbol type)
                  (setf (symbol-type-definition symbol) (map-type type))))))))
      
      (multiple-value-call
       #'gen-cliques
       (gen-cliques-1 #'(lambda (type) (cons (type-kind type) (type-tags type)))))
      (recompute-type-caches world))))


;;; ------------------------------------------------------------------------------------------------------
;;; SPECIALS


(defun checked-callable (f)
  (let ((fun (callable f)))
    (unless fun
      (warn "Undefined function ~S" f))
    fun))


; Add a macro, command, or special form definition.  symbol is a symbol that names the
; preprocessor directive, macro, command, or special form.  When a semantic form
;   (id arg1 arg2 ... argn)
; is encountered and id is a symbol with the same name as symbol, the form is
; replaced by the result of calling one of:
;   (expander preprocessor-state id arg1 arg2 ... argn)           if property is :preprocess
;   (expander world type-env arg1 arg2 ... argn)                  if property is :macro
;   (expander world grammar-info-var arg1 arg2 ... argn)          if property is :command
;   (expander world type-env id arg1 arg2 ... argn)               if property is :special-form
;   (expander world allow-forward-references arg1 arg2 ... argn)  if property is :type-constructor
; expander must be a function or a function symbol.
;
; depictor is used instead of expander when emitting markup for the macro, command, or special form.
; depictor is called via:
;   (depictor markup-stream world level arg1 arg2 ... argn)          if property is :macro
;   (depictor markup-stream world depict-env arg1 arg2 ... argn)     if property is :command
;   (depictor markup-stream world level arg1 arg2 ... argn)          if property is :special-form
;   (depictor markup-stream world level arg1 arg2 ... argn)          if property is :type-constructor
;
(defun add-special (property symbol expander &optional depictor)
  (let ((emit-property (cdr (assoc property '((:macro . :depict-macro)
                                              (:command . :depict-command)
                                              (:special-form . :depict-special-form)
                                              (:type-constructor . :depict-type-constructor))))))
    (assert-true (or emit-property (not depictor)))
    (assert-type symbol identifier)
    (when *value-asserts*
      (checked-callable expander)
      (when depictor (checked-callable depictor)))
    (when (or (get symbol property) (and emit-property (get symbol emit-property)))
      (error "Attempt to redefine ~A ~A" property symbol))
    (setf (get symbol property) expander)
    (when emit-property
      (if depictor
        (setf (get symbol emit-property) depictor)
        (remprop symbol emit-property)))
    (export-symbol symbol)))


;;; ------------------------------------------------------------------------------------------------------
;;; PRIMITIVES

(defstruct (primitive (:constructor make-primitive (type-expr value-code appearance &key markup1 markup2 level level1 level2))
                      (:predicate primitive?))
  (type nil :type (or null type))          ;Type of this primitive; nil if not computed yet
  (type-expr nil :read-only t)             ;Source type expression that designates the type of this primitive
  (value-code nil :read-only t)            ;Lisp expression that computes the value of this primitive
  (appearance nil :read-only t)            ;One of the possible primitive appearances (see below)
  (markup1 nil :read-only t)               ;Markup (item or list) for this primitive
  (markup2 nil :read-only t)               ;:global primitives:  name to use for an external reference
  ;                                        ;:unary primitives:   markup (item or list) for this primitive's closer
  ;                                        ;:infix primitives:   true if spaces should be put around primitive
  (level nil :read-only t)                 ;Precedence level of markup for this primitive
  (level1 nil :read-only t)                ;Precedence level required for first argument of this primitive
  (level2 nil :read-only t))               ;Precedence level required for second argument of this primitive

;appearance is one of the following:
; :global      The primitive appears as a regular, global function or constant; its markup is in markup1.
;                If this primitive should generate an external reference, markup2 contains the name to use for the reference
; :infix       The primitive is an infix binary primitive; its markup is in markup1; if markup2 is true, put spaces around markup1
; :unary       The primitive is a prefix and/or suffix unary primitive; the prefix is in markup1 and suffix in markup2
; :phantom     The primitive disappears when emitting markup for it


; Call this to declare all primitives when initially constructing a world,
; before types have been constructed.
(defun declare-primitive (symbol type-expr value-code appearance &rest key-args)
  (when (symbol-primitive symbol)
    (error "Attempt to redefine primitive ~A" symbol))
  (setf (symbol-primitive symbol) (apply #'make-primitive type-expr value-code appearance key-args))
  (export-symbol symbol))


; Call this to compute the primitive's type from its type-expr.
(defun define-primitive (world primitive)
  (setf (primitive-type primitive) (scan-type world (primitive-type-expr primitive))))


; If name is an identifier not already used by a special form, command, primitive, or macro,
; return it interened into the world's package.  If not, generate an error.
(defun scan-name (world name)
  (unless (identifier? name)
    (error "~S should be an identifier" name))
  (let ((symbol (world-intern world name)))
    (when (get-properties (symbol-plist symbol) '(:command :special-form :primitive :macro :type-constructor))
      (error "~A is reserved" symbol))
    symbol))


;;; ------------------------------------------------------------------------------------------------------
;;; TYPE ENVIRONMENTS

;;; A type environment is an alist that associates bound variables with their types.
;;; A variable may be bound multiple times; the first binding in the environment list
;;; shadows ones further in the list.
;;; The following kinds of bindings are allowed in a type environment:
;;;
;;;   (symbol . type)
;;;   Normal local variable, where:
;;;     symbol is a world-interned name of the local variable;
;;;     type is that variable's type.
;;;
;;;   (:lhs-symbol . symbol)
;;;   The lhs nonterminal's symbol if this is a type environment for an action function.
;;;
;;;   ((action symbol . index) local-symbol type general-grammar-symbol)
;;;   Action variable, where:
;;;     action is a world-interned symbol denoting the action function being called
;;;     symbol is a terminal or nonterminal's symbol on which the action is called
;;;     index is the one-based index used to distinguish among identical
;;;       symbols in the rhs of a production.  The first occurrence of this
;;;       symbol has index 1, the second has index 2, and so on.
;;;     local-symbol is a unique local variable name used to represent the action
;;;       function's value in the generated lisp code
;;;     type is the type of the action function's value
;;;     general-grammar-symbol is the general-grammar-symbol corresponding to the index-th
;;;       instance of symbol in the production's rhs
;;;
;;;   (:no-code-gen)
;;;   If present, this indicates that the code returned from this scan-value or related call
;;;   will be discarded; only the type is important.  This flag is used as an optimization.

(defconstant *null-type-env* nil)


; If symbol is a local variable, return two values:
;   the name to use to refer to it from the generated lisp code;
;   the variable's type.
; Otherwise, return nil.
; symbol must already be world-interned.
(declaim (inline type-env-local))
(defun type-env-local (type-env symbol)
  (let ((binding (assoc symbol type-env :test #'eq)))
    (when binding
      (values (car binding) (cdr binding)))))


; If the currently generated function is an action, return that action production's
; lhs nonterminal's symbol; otherwise return nil.
(defun type-env-lhs-symbol (type-env)
  (cdr (assoc ':lhs-symbol type-env :test #'eq)))


; If the currently generated function is an action for a rule with at least index
; instances of the given grammar-symbol's symbol on the right-hand side, and if action is
; a legal action for that symbol, return three values:
;   the name to use from the generated lisp code to refer to the result of calling
;     the action on the index-th instance of this symbol;
;   the action result's type;
;   the general-grammar-symbol corresponding to the index-th instance of this symbol in the rhs.
; Otherwise, return nil.
; action must already be world-interned.
(defun type-env-action (type-env action symbol index)
  (let ((binding (assoc (list* action symbol index) type-env :test #'equal)))
    (when binding
      (values (second binding) (third binding) (fourth binding)))))


; Append bindings to the front of the type-env.  The bindings list is destroyed.
(declaim (inline type-env-add-bindings))
(defun type-env-add-bindings (type-env bindings)
  (nconc bindings type-env))


; Return an environment obtained from the type-env by adding a :no-code-gen binding.
(defun inhibit-code-gen (type-env)
  (cons (list ':no-code-gen) type-env))


; Return true if the type-env indicates that its code will be discarded.
(defun code-gen-inhibited (type-env)
  (assoc ':no-code-gen type-env))


;;; ------------------------------------------------------------------------------------------------------
;;; VALUES

;;; A value is one of the following:
;;;   A void value (represented by nil)
;;;   A boolean (nil for false; non-nil for true)
;;;   An integer
;;;   A rational number
;;;   A double-precision floating-point number (or :+inf, :-inf, or :nan)
;;;   A character
;;;   A function (represented by a lisp function)
;;;   A vector (represented by a list)
;;;   A set (represented by an intset of its elements converted to integers)
;;;   A tuple (represented by a list of elements' values)
;;;   A oneof (represented by a pair: tag . value)
;;;   An address (represented by a cons cell whose cdr contains the value and car contains a serial number)


(defvar *address-counter*) ;Last used address serial number


; Return true if the value appears to have the given type.  This function
; may return false positives (return true when the value doesn't actually
; have the given type) but never false negatives.
; If shallow is true, only test at the top level.
(defun value-has-type (value type &optional shallow)
  (case (type-kind type)
    (:bottom nil)
    (:void (null value))
    (:boolean t)
    (:integer (integerp value))
    (:rational (rationalp value))
    (:double (double? value))
    (:character (characterp value))
    (:-> (functionp value))
    (:vector (let ((element-type (vector-element-type type)))
               (if (eq (type-kind element-type) :character)
                 (stringp value)
                 (labels
                   ((test (value)
                      (or (null value)
                          (and (consp value)
                               (or shallow (value-has-type (car value) element-type))
                               (test (cdr value))))))
                   (test value)))))
    (:set (valid-intset? value))
    (:tuple (labels
              ((test (value types)
                 (or (and (null value) (null types))
                     (and (consp value)
                          (consp types)
                          (or shallow (value-has-type (car value) (car types)))
                          (test (cdr value) (cdr types))))))
              (test value (type-parameters type))))
    (:oneof (and (consp value)
                 (let ((field-type (field-type type (car value))))
                   (and field-type
                        (or shallow (value-has-type (cdr value) field-type))))))
    (:address (and (consp value)
                   (integerp (car value))
                   (or shallow (value-has-type (cdr value) (address-element-type type)))))
    (t (error "Bad typekind ~S" (type-kind type)))))


; Print the value nicely on the given stream.  type is the value's type.
(defun print-value (value type &optional (stream t))
  (assert-true (value-has-type value type t))
  (case (type-kind type)
    (:void (assert-true (null value))
           (write-string "empty" stream))
    (:boolean (write-string (if value "true" "false") stream))
    ((:integer :rational :character :->) (write value :stream stream))
    (:double (case value
               (:+inf (write-string "+infinity" stream))
               (:-inf (write-string "-infinity" stream))
               (:nan (write-string "NaN" stream))
               (t (write value :stream stream))))
    (:vector (let ((element-type (vector-element-type type)))
               (if (eq (type-kind element-type) :character)
                 (prin1 value stream)
                 (pprint-logical-block (stream value :prefix "(" :suffix ")")
                   (pprint-exit-if-list-exhausted)
                   (loop
                     (print-value (pprint-pop) element-type stream)
                     (pprint-exit-if-list-exhausted)
                     (format stream " ~:_"))))))
    (:set (let ((converter (set-out-converter (set-element-type type))))
            (pprint-logical-block (stream value :prefix "{" :suffix "}")
              (pprint-exit-if-list-exhausted)
              (loop
                (let* ((values (pprint-pop))
                       (value1 (car values))
                       (value2 (cdr values)))
                  (if (= value1 value2)
                    (write (funcall converter value1) :stream stream)
                    (write (list (funcall converter value1) (funcall converter value2)) :stream stream))))
              (pprint-exit-if-list-exhausted)
              (format stream " ~:_"))))
    (:tuple (print-values value (type-parameters type) stream :prefix "[" :suffix "]"))
    (:oneof (pprint-logical-block (stream nil :prefix "{" :suffix "}")
              (let* ((tag (car value))
                     (field-type (field-type type tag)))
                (format stream "~A" tag)
                (unless (eq (type-kind field-type) :void)
                  (format stream " ~:_")
                  (print-value (cdr value) field-type stream)))))
    (:address (pprint-logical-block (stream nil :prefix "{" :suffix "}")
                (format stream "~D ~:_" (car value))
                (print-value (cdr value) (address-element-type type) stream)))
    (t (error "Bad typekind ~S" (type-kind type)))))


; Print a list of values nicely on the given stream.  types is the list of the
; values' types (and should have the same length as the list of values).
; If prefix and/or suffix are non-null, use them as beginning and ending
; delimiters of the printed list.
(defun print-values (values types &optional (stream t) &key prefix suffix)
  (assert-true (= (length values) (length types)))
  (pprint-logical-block (stream values :prefix prefix :suffix suffix)
    (pprint-exit-if-list-exhausted)
    (dolist (type types)
      (print-value (pprint-pop) type stream)
      (pprint-exit-if-list-exhausted)
      (format stream " ~:_"))))


;;; ------------------------------------------------------------------------------------------------------
;;; VALUE EXPRESSIONS

;;; Expressions are annotated to avoid having to duplicate the expression scanning logic when
;;; emitting markup for expressions.  Expression forms are prefixed with an expr-annotation symbol
;;; to indicate their kinds.  These symbols are in their own package to avoid potential confusion
;;; with keywords, variable names, terminals, etc.
;;;
;;; Some special forms are extended to include parsed type information for the benefit of markup logic.
(eval-when (:compile-toplevel :load-toplevel :execute)
  (defpackage "EXPR-ANNOTATION"
    (:use)
    (:export "CONSTANT"     ;(expr-annotation:constant <constant>)
             "PRIMITIVE"    ;(expr-annotation:primitive <interned-id>)
             "LOCAL"        ;(expr-annotation:local <interned-id>)      ;Local or lexically scoped variable
             "GLOBAL"       ;(expr-annotation:global <interned-id>)     ;Global variable
             "CALL"         ;(expr-annotation:call <function-expr> <arg-expr> ... <arg-expr>)
             "ACTION"       ;(expr-annotation:action <action> <general-grammar-symbol> <optional-index>)
             "SPECIAL-FORM" ;(expr-annotation:special-form <interned-form> ...)
             "MACRO")))     ;(expr-annotation:macro <interned-macro> <expansion-expr>)


; Return true if the annotated-expr is a special form annotated expression with
; the given special-form.  special-form must be a symbol but does not have to be interned
; in the world's package.
(defun special-form-annotated-expr? (special-form annotated-expr)
  (and (eq (first annotated-expr) 'expr-annotation:special-form)
       (string= (symbol-name (second annotated-expr)) (symbol-name special-form))))


; Return true if the annotated-expr is a macro annotated expression with the given macro.
; macro must be a symbol but does not have to be interned in the world's package.
(defun macro-annotated-expr? (macro annotated-expr)
  (and (eq (first annotated-expr) 'expr-annotation:macro)
       (string= (symbol-name (second annotated-expr)) (symbol-name macro))))


; Return the value of the variable with the given symbol.
; Compute the value if the variable was unbound.
; Use the *busy-variables* list to prevent infinite recursion while computing variable values.
(defmacro fetch-value (symbol)
  `(if (boundp ',symbol)
     (symbol-value ',symbol)
     (compute-variable-value ',symbol)))


; Generate a lisp expression that will compute the value of value-expr.
; type-env is the type environment.  The expression may refer to free variables
; present in the type-env.
; Return three values:
;   The expression's value (a lisp expression)
;   The expression's type
;   The annotated value-expr
(defun scan-value (world type-env value-expr)
  (labels
    ((syntax-error ()
       (error "Syntax error: ~S" value-expr))
     
     ;Scan a function call.  The function has already been scanned into its value and type,
     ;but the arguments are still unprocessed.
     (scan-call (function-value function-type function-annotated-expr arg-exprs)
       (let ((arg-values nil)
             (arg-types nil)
             (arg-annotated-exprs nil))
         (dolist (arg-expr arg-exprs)
           (multiple-value-bind (arg-value arg-type arg-annotated-expr) (scan-value world type-env arg-expr)
             (push arg-value arg-values)
             (push arg-type arg-types)
             (push arg-annotated-expr arg-annotated-exprs)))
         (let ((arg-values (nreverse arg-values))
               (arg-types (nreverse arg-types))
               (arg-annotated-exprs (nreverse arg-annotated-exprs)))
           (unless (and (eq (type-kind function-type) :->)
                        (= (length arg-types) (length (->-argument-types function-type)))
                        (every #'type<= arg-types (->-argument-types function-type)))
             (error "~@<Call type mismatch in ~S: ~_Function of type ~A called with arguments of types~:_~{ ~A~}~:>"
                    value-expr
                    (print-type-to-string function-type)
                    (mapcar #'print-type-to-string arg-types)))
           (values (apply #'gen-apply function-value arg-values)
                   (->-result-type function-type)
                   (list* 'expr-annotation:call function-annotated-expr arg-annotated-exprs)))))
     
     ;Scan an action call
     (scan-action-call (action symbol &optional (index 1 index-supplied))
       (unless (integerp index)
         (error "Production rhs grammar symbol index ~S must be an integer" index))
       (multiple-value-bind (symbol-code symbol-type general-grammar-symbol) (type-env-action type-env action symbol index)
         (unless symbol-code
           (error "Action ~S not found" (list action symbol index)))
         (let ((multiple-symbols (type-env-action type-env action symbol 2)))
           (when (and (not index-supplied) multiple-symbols)
             (error "Ambiguous index in action ~S" (list action symbol)))
           (values symbol-code
                   symbol-type
                   (list* 'expr-annotation:action action general-grammar-symbol
                          (and (or multiple-symbols
                                   (grammar-symbol-= symbol (assert-non-null (type-env-lhs-symbol type-env))))
                               (list index)))))))
     
     ;Scan an interned identifier
     (scan-identifier (symbol)
       (multiple-value-bind (symbol-code symbol-type) (type-env-local type-env symbol)
         (if symbol-code
           (values symbol-code symbol-type (list 'expr-annotation:local symbol))
           (let ((primitive (symbol-primitive symbol)))
             (if primitive
               (values (primitive-value-code primitive) (primitive-type primitive) (list 'expr-annotation:primitive symbol))
               (let ((type (symbol-type symbol)))
                 (if type
                   (values (list 'fetch-value symbol) type (list 'expr-annotation:global symbol))
                   (syntax-error))))))))
     
     ;Scan a call or macro expansion
     (scan-cons (first rest)
       (if (identifier? first)
         (let* ((symbol (world-intern world first))
                (expander (symbol-macro symbol)))
           (if expander
             (multiple-value-bind (expansion-code expansion-type expansion-annotated-expr)
                                  (scan-value world type-env (apply expander world type-env rest))
               (values
                expansion-code
                expansion-type
                (list 'expr-annotation:macro symbol expansion-annotated-expr)))
             (let ((handler (get symbol :special-form)))
               (if handler
                 (apply handler world type-env symbol rest)
                 (if (and (symbol-action symbol) (not (type-env-local type-env symbol)))
                   (apply #'scan-action-call symbol rest)
                   (multiple-value-call #'scan-call (scan-identifier symbol) rest))))))
         (multiple-value-call #'scan-call (scan-value world type-env first) rest)))
     
     (scan-constant (value-expr type)
       (values value-expr type (list 'expr-annotation:constant value-expr))))
    
    (assert-three-values
     (cond
      ((consp value-expr) (scan-cons (first value-expr) (rest value-expr)))
      ((identifier? value-expr) (scan-identifier (world-intern world value-expr)))
      ((integerp value-expr) (scan-constant value-expr (world-integer-type world)))
      ((floatp value-expr) (scan-constant value-expr (world-double-type world)))
      ((characterp value-expr) (scan-constant value-expr (world-character-type world)))
      ((stringp value-expr) (scan-constant value-expr (world-string-type world)))
      (t (syntax-error))))))


; Same as scan-value except that return only the expression's type.
(defun scan-value-type (world type-env value-expr)
  (nth-value 1 (scan-value world (inhibit-code-gen type-env) value-expr)))


; Same as scan-value except that ensure that the value has the expected type.
; Return two values:
;   The expression's value (a lisp expression)
;   The annotated value-expr
(defun scan-typed-value (world type-env value-expr expected-type)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (unless (type<= type expected-type)
      (error "Expected type ~A for ~:W but got type ~A"
             (print-type-to-string expected-type)
             value-expr
             (print-type-to-string type)))
    (values value annotated-expr)))


; Same as scan-value except that ensure that the value has the expected type kind.
; Return three values:
;   The expression's value (a lisp expression)
;   The expression's type
;   The annotated value-expr
(defun scan-kinded-value (world type-env value-expr expected-type-kind)
  (multiple-value-bind (value type annotated-expr) (scan-value world type-env value-expr)
    (unless (typekind<= (type-kind type) expected-type-kind)
      (error "Expected ~(~A~) for ~:W but got type ~A"
             expected-type-kind
             value-expr
             (print-type-to-string type)))
    (values value type annotated-expr)))


(defvar *busy-variables* nil)

; Compute the value of a world's variable named by symbol.  Return two values:
;   The variable's value
;   The variable's type
; If the variable already has a computed value, return it unchanged.
; If computing the value requires the values of other variables, compute them as well.
; Use the *busy-variables* list to prevent infinite recursion while computing variable values.
(defun compute-variable-value (symbol)
  (cond
   ((member symbol *busy-variables*) (error "Definition of ~A refers to itself" symbol))
   ((boundp symbol) (values (symbol-value symbol) (symbol-type symbol)))
   (t (let* ((*busy-variables* (cons symbol *busy-variables*))
             (value-expr (get symbol :value-expr)))
        (handler-bind (((or error warning)
                        #'(lambda (condition)
                            (declare (ignore condition))
                            (format *error-output* "~&~@<~2IWhile computing ~A: ~_~:W~:>~%"
                                    symbol value-expr))))
          (multiple-value-bind (value-code type) (scan-value (symbol-world symbol) *null-type-env* value-expr)
            (unless (type<= type (symbol-type symbol))
              (error "~A evaluates to type ~A, but is defined with type ~A"
                     symbol
                     (print-type-to-string type)
                     (print-type-to-string (symbol-type symbol))))
            (let ((named-value-code (name-lambda value-code symbol)))
              (setf (symbol-code symbol) named-value-code)
              (when *trace-variables*
                (format *trace-output* "~&~S := ~:W~%" symbol named-value-code))
              (values (set symbol (eval named-value-code)) type))))))))


; Compute the initial type-env to use for the given general-production's action code.
; The first cell of the type-env gives the production's lhs nonterminal's symbol;
; the remaining cells give the action arguments in order.
(defun general-production-action-env (grammar general-production)
  (let* ((current-indices nil)
         (lhs-general-nonterminal (general-production-lhs general-production))
         (bound-arguments-alist (nonterminal-sample-bound-argument-alist grammar lhs-general-nonterminal)))
    (acons ':lhs-symbol (general-grammar-symbol-symbol lhs-general-nonterminal)
           (mapcan
            #'(lambda (general-grammar-symbol)
                (let* ((symbol (general-grammar-symbol-symbol general-grammar-symbol))
                       (index (incf (getf current-indices symbol 0)))
                       (grammar-symbol (instantiate-general-grammar-symbol bound-arguments-alist general-grammar-symbol)))
                  (mapcar
                   #'(lambda (declaration)
                       (let* ((action-symbol (car declaration))
                              (action-type (cdr declaration))
                              (local-symbol (gensym (symbol-name action-symbol))))
                         (list
                          (list* action-symbol symbol index)
                          local-symbol
                          action-type
                          general-grammar-symbol)))
                   (grammar-symbol-signature grammar grammar-symbol))))
            (general-production-rhs general-production)))))


; Return the number of arguments that a function returned by compute-action-code
; would expect.
(defun n-action-args (grammar production)
  (let ((n-args 0))
    (dolist (grammar-symbol (production-rhs production))
      (incf n-args (length (grammar-symbol-signature grammar grammar-symbol))))
    n-args))


; Compute the code for evaluating body-expr to obtain the value of one of the
; production's actions.  Verify that the result has the given type.
; The code is a lambda-expression that takes as arguments the results of all
; defined actions on the production's rhs.  The arguments are listed in the
; same order as the grammar symbols in the rhs.  If a grammar symbol in the rhs
; has more than one associated action, arguments are used corresponding to all
; of the actions in the same order as they were declared.  If a grammar symbol
; in the rhs has no associated actions, no argument is used for it.
(defun compute-action-code (world grammar production action-symbol body-expr type)
  (handler-bind ((error #'(lambda (condition)
                            (declare (ignore condition))
                            (format *error-output* "~&~@<~2IWhile processing action ~A on ~S: ~_~:W~:>~%"
                                    action-symbol (production-name production) body-expr))))
    (let* ((initial-env (general-production-action-env grammar production))
           (args (mapcar #'cadr (cdr initial-env)))
           (body-code (scan-typed-value world initial-env body-expr type))
           (named-body-code (name-lambda body-code
                                         (concatenate 'string (symbol-name (production-name production))
                                                      "~" (symbol-name action-symbol))
                                         (world-package world))))
      (gen-lambda args named-body-code))))


; Return a list of all grammar symbols's symbols that are present in at least one expr-annotation:action
; in the annotated expression.  The symbols are returned in no particular order.
(defun annotated-expr-grammar-symbols (annotated-expr)
  (let ((symbols nil))
    (labels
      ((scan (annotated-expr)
         (when (consp annotated-expr)
           (if (eq (first annotated-expr) 'expr-annotation:action)
             (pushnew (general-grammar-symbol-symbol (third annotated-expr)) symbols :test *grammar-symbol-=*)
             (mapc #'scan annotated-expr)))))
      (scan annotated-expr)
      symbols)))


;;; ------------------------------------------------------------------------------------------------------
;;; SPECIAL FORMS

;;; Control structures

(defun eval-bottom ()
  (error "Reached a BOTTOM statement"))

; (bottom)
; Raises an error.
(defun scan-bottom (world type-env special-form)
  (declare (ignore type-env))
  (values
   '(eval-bottom)
   (world-bottom-type world)
   (list 'expr-annotation:special-form special-form)))


; (function ((<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <body>)
(defun scan-function (world type-env special-form arg-binding-exprs body-expr)
  (flet
    ((scan-arg-binding (arg-binding-expr)
       (unless (and (consp arg-binding-expr)
                    (consp (cdr arg-binding-expr))
                    (member (cddr arg-binding-expr) '(nil (:unused)) :test #'equal))
         (error "Bad function binding ~S" arg-binding-expr))
       (let ((arg-symbol (scan-name world (first arg-binding-expr)))
             (arg-type (scan-type world (second arg-binding-expr))))
         (cons arg-symbol arg-type))))
    
    (unless (listp arg-binding-exprs)
      (error "Bad function bindings ~S" arg-binding-exprs))
    (let* ((arg-bindings (mapcar #'scan-arg-binding arg-binding-exprs))
           (args (mapcar #'car arg-bindings))
           (arg-types (mapcar #'cdr arg-bindings))
           (unused-args (mapcan #'(lambda (arg arg-binding-expr)
                                    (when (eq (third arg-binding-expr) ':unused)
                                      (list arg)))
                                args arg-binding-exprs))
           (type-env (type-env-add-bindings type-env arg-bindings)))
      (multiple-value-bind (body-code body-type body-annotated-expr) (scan-value world type-env body-expr)
        (values (if unused-args
                  `#'(lambda ,args (declare (ignore . ,unused-args)) ,body-code)
                  `#'(lambda ,args ,body-code))
                (make-->-type world arg-types body-type)
                (list 'expr-annotation:special-form special-form arg-binding-exprs body-annotated-expr))))))


; (if <condition-expr> <true-expr> <false-expr>)
(defun scan-if (world type-env special-form condition-expr true-expr false-expr)
  (multiple-value-bind (condition-code condition-annotated-expr)
                       (scan-typed-value world type-env condition-expr (world-boolean-type world))
    (multiple-value-bind (true-code true-type true-annotated-expr) (scan-value world type-env true-expr)
      (multiple-value-bind (false-code false-type false-annotated-expr) (scan-value world type-env false-expr)
        (let ((join-type (type-lub true-type false-type)))
          (unless join-type
            (error "~S: ~A and ~S: ~A used as alternatives in an if"
                   true-expr (print-type-to-string true-type)
                   false-expr (print-type-to-string false-type)))
          (values
           (list 'if condition-code true-code false-code)
           join-type
           (list 'expr-annotation:special-form special-form condition-annotated-expr true-annotated-expr false-annotated-expr)))))))


(defconstant *semantic-exception-type-name* 'semantic-exception)

; (throw <value-expr>)
; <value-expr> must have type *semantic-exception-type-name*, which must be the name of some user-defined type in the environment.
(defun scan-throw (world type-env special-form value-expr)
  (multiple-value-bind (value-code value-annotated-expr)
                       (scan-typed-value world type-env value-expr (scan-type world *semantic-exception-type-name*))
    (values
     (list 'throw ':semantic-exception value-code)
     (world-bottom-type world)
     (list 'expr-annotation:special-form special-form value-annotated-expr))))


; (catch <body> (<var> [:unused]) <handler>)
(defun scan-catch (world type-env special-form body-expr arg-binding-expr handler-expr)
  (multiple-value-bind (body-code body-type body-annotated-expr) (scan-value world type-env body-expr)
    (unless (and (consp arg-binding-expr)
                 (member (cdr arg-binding-expr) '(nil (:unused)) :test #'equal))
      (error "Bad catch binding ~S" arg-binding-expr))
    (let* ((arg-symbol (scan-name world (first arg-binding-expr)))
           (arg-type (scan-type world *semantic-exception-type-name*))
           (arg-bindings (list (cons arg-symbol arg-type)))
           (type-env (type-env-add-bindings type-env arg-bindings)))
      (multiple-value-bind (handler-code handler-type handler-annotated-expr) (scan-value world type-env handler-expr)
        (let ((join-type (type-lub body-type handler-type)))
          (unless join-type
            (error "~S: ~A and ~S: ~A used as alternatives in a catch"
                   body-expr (print-type-to-string body-type)
                   handler-expr (print-type-to-string handler-type)))
          (values
           `(block nil
              (let ((,arg-symbol (catch ':semantic-exception (return ,body-code))))
                ,@(and (eq (second arg-binding-expr) ':unused) `((declare (ignore ,arg-symbol))))
                ,handler-code))
           join-type
           (list 'expr-annotation:special-form special-form body-annotated-expr arg-binding-expr handler-annotated-expr)))))))


;;; Vectors

(defmacro non-empty-vector (v operation-name)
  `(or ,v (error ,(concatenate 'string operation-name " called on empty vector"))))

; (vector <element-expr> <element-expr> ... <element-expr>)
; Makes a vector of one or more elements.
(defun scan-vector-form (world type-env special-form element-expr &rest element-exprs)
  (multiple-value-bind (element-code element-type element-annotated-expr) (scan-value world type-env element-expr)
    (multiple-value-map-bind (rest-codes rest-annotated-exprs)
                             #'(lambda (element-expr)
                                 (scan-typed-value world type-env element-expr element-type))
                             (element-exprs)
      (let ((elements-code (list* 'list element-code rest-codes)))
        (values
         (if (eq element-type (world-character-type world))
           (if element-exprs
             (list 'coerce elements-code ''string)
             (list 'string element-code))
           elements-code)
         (make-vector-type world element-type)
         (list* 'expr-annotation:special-form special-form element-annotated-expr rest-annotated-exprs))))))


; (vector-of <element-type>)
; Makes a zero-element vector of elements of the given type.
(defun scan-vector-of (world type-env special-form element-type-expr)
  (declare (ignore type-env))
  (let ((element-type (scan-type world element-type-expr)))
    (values
     (if (eq element-type (world-character-type world))
       ""
       nil)
     (make-vector-type world element-type)
     (list 'expr-annotation:special-form special-form element-type-expr))))


; (empty <vector-expr>)
; Returns true if the vector has zero elements.
; This is equivalent to (= (length <vector-expr>) 0) and depicts the same as the latter but
; is implemented more efficiently.
(defun scan-empty (world type-env special-form vector-expr)
  (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-kinded-value world type-env vector-expr :vector)
    (values
     (if (eq vector-type (world-string-type world))
       `(= (length ,vector-code) 0)
       (list 'endp vector-code))
     (world-boolean-type world)
     (list 'expr-annotation:special-form special-form vector-annotated-expr))))


; (length <vector-expr>)
; Returns the number of elements in the vector.
(defun scan-length (world type-env special-form vector-expr)
  (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-kinded-value world type-env vector-expr :vector)
    (declare (ignore vector-type))
    (values
     (list 'length vector-code)
     (world-integer-type world)
     (list 'expr-annotation:special-form special-form vector-annotated-expr))))


; (nth <vector-expr> <n-expr>)
; Returns the nth element of the vector.  Throws an error if the vector's length is less than n.
(defun scan-nth (world type-env special-form vector-expr n-expr)
  (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-kinded-value world type-env vector-expr :vector)
    (multiple-value-bind (n-code n-annotated-expr) (scan-typed-value world type-env n-expr (world-integer-type world))
      (values
       (cond
        ((eq vector-type (world-string-type world))
         `(char ,vector-code ,n-code))
        ((eql n-code 0)
         `(car (non-empty-vector ,vector-code "first")))
        (t (let ((n (gensym "N")))
             `(let ((,n ,n-code))
                (car (non-empty-vector (nthcdr ,n ,vector-code) "nth"))))))
       (vector-element-type vector-type)
       (list 'expr-annotation:special-form special-form vector-annotated-expr n-annotated-expr)))))


; (subseq <vector-expr> <low-expr> [<high-expr>])
; Returns a vector containing elements of the given vector from low-expr to high-expr inclusive.
; high-expr defaults to length-1.
; It is required that 0 <= low-expr <= high-expr+1 <= length.
(defun scan-subseq (world type-env special-form vector-expr low-expr &optional high-expr)
  (let ((integer-type (world-integer-type world)))
    (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-kinded-value world type-env vector-expr :vector)
      (multiple-value-bind (low-code low-annotated-expr) (scan-typed-value world type-env low-expr integer-type)
        (if high-expr
          (multiple-value-bind (high-code high-annotated-expr) (scan-typed-value world type-env high-expr integer-type)
            (values
             `(subseq ,vector-code ,low-code (1+ ,high-code))
             vector-type
             (list 'expr-annotation:special-form special-form vector-annotated-expr low-annotated-expr high-annotated-expr)))
          (values
           (case low-code
             (0 vector-code)
             (1 (if (eq vector-type (world-string-type world))
                  `(subseq ,vector-code 1)
                  `(cdr (non-empty-vector ,vector-code "rest"))))
             (t `(subseq ,vector-code ,low-code)))
           vector-type
           (list 'expr-annotation:special-form special-form vector-annotated-expr low-annotated-expr nil)))))))


; (append <vector-expr> <vector-expr>)
; Returns a vector contatenating the two given vectors, which must have the same element type.
(defun scan-append (world type-env special-form vector1-expr vector2-expr)
  (multiple-value-bind (vector1-code vector-type vector1-annotated-expr) (scan-kinded-value world type-env vector1-expr :vector)
    (multiple-value-bind (vector2-code vector2-annotated-expr) (scan-typed-value world type-env vector2-expr vector-type)
      (values
       (if (eq vector-type (world-string-type world))
         `(concatenate 'string ,vector1-code ,vector2-code)
         (list 'append vector1-code vector2-code))
       vector-type
       (list 'expr-annotation:special-form special-form vector1-annotated-expr vector2-annotated-expr)))))


; (set-nth <vector-expr> <n-expr> <value-expr>)
; Returns a vector containing the same elements of the given vector except that the nth has been replaced
; with value-expr.  n must be between 0 and length-1, inclusive.
(defun scan-set-nth (world type-env special-form vector-expr n-expr value-expr)
  (multiple-value-bind (vector-code vector-type vector-annotated-expr) (scan-kinded-value world type-env vector-expr :vector)
    (multiple-value-bind (n-code n-annotated-expr) (scan-typed-value world type-env n-expr (world-integer-type world))
      (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr (vector-element-type vector-type))
        (values
         (let ((vector (gensym "V"))
               (n (gensym "N")))
           `(let ((,vector ,vector-code)
                  (,n ,n-code))
              (if (or (< ,n 0) (>= ,n (length ,vector)))
                (error "Range error")
                ,(if (eq vector-type (world-string-type world))
                   `(progn
                      (setq ,vector (copy-seq ,vector))
                      (setf (char ,vector ,n) ,value-code)
                      ,vector)
                   (let ((l (gensym "L")))
                     `(let ((,l (nthcdr ,n ,vector)))
                        (append (ldiff ,vector ,l)
                                (cons ,value-code (cdr ,l)))))))))
         vector-type
         (list 'expr-annotation:special-form special-form vector-annotated-expr n-annotated-expr value-annotated-expr))))))


;;; Sets

; Return a function that converts values of the given element-type to integers for storage in a set.
(defun set-in-converter (element-type)
  (ecase (type-kind element-type)
    (:integer #'identity)
    (:character #'char-code)))


; expr is the source code of an expression that generates a value of the given element-type.  Return
; the source code of an expression that generates the corresponding integer for storage in a set of
; the given element-type.
(defun set-in-converter-expr (element-type expr)
  (ecase (type-kind element-type)
    (:integer expr)
    (:character (list 'char-code expr))))


; Return a function that converts integers to values of the given element-type for retrieval from a set.
(defun set-out-converter (element-type)
  (ecase (type-kind element-type)
    (:integer #'identity)
    (:character #'code-char)))


; (set-of-ranges <element-type> <low-expr> <high-expr> ... <low-expr> <high-expr>)
; Makes a set of zero or more elements or element ranges.  Each <high-expr> can be null to indicate a
; one-element range.
(defun scan-set-of-ranges (world type-env special-form element-type-expr &rest element-exprs)
  (let* ((element-type (scan-type world element-type-expr))
         (high t))
    (multiple-value-map-bind (element-codes element-annotated-exprs)
                             #'(lambda (element-expr)
                                 (setq high (not high))
                                 (if (and high (null element-expr))
                                   (values nil nil)
                                   (multiple-value-bind (element-code element-annotated-expr)
                                                        (scan-typed-value world type-env element-expr element-type)
                                     (values (set-in-converter-expr element-type element-code)
                                             element-annotated-expr))))
                             (element-exprs)
      (unless high
        (error "Odd number of set-of-ranges elements: ~S" element-exprs))
      (values
       (cons 'intset-from-ranges element-codes)
       (make-set-type world element-type)
       (list* 'expr-annotation:special-form special-form element-type-expr element-annotated-exprs)))))


;;; Oneofs

; (oneof <tag> <value-expr>)
; oneof-type is inferred from the tag.
(defun scan-oneof-form (world type-env special-form tag &optional (value-expr nil has-value-expr))
  (multiple-value-bind (value-code value-type value-annotated-expr)
                       (if has-value-expr
                         (scan-value world type-env value-expr)
                         (values nil (world-void-type world) nil))
    (let ((type (lookup-oneof-tag world tag value-type)))
      (values
       `(cons ',tag ,value-code)
       type
       (list 'expr-annotation:special-form special-form tag value-annotated-expr type)))))


; (typed-oneof <type-expr> <tag> <value-expr>)
(defun scan-typed-oneof (world type-env special-form type-expr tag &optional (value-expr nil has-value-expr))
  (let ((type (scan-kinded-type world type-expr :oneof)))
    (multiple-value-bind (tag field-type) (scan-tag type tag)
      (multiple-value-bind (value-code value-annotated-expr)
                           (cond
                            (has-value-expr (scan-typed-value world type-env value-expr field-type))
                            ((eq (type-kind field-type) :void) (values nil nil))
                            (t (error "Missing oneof value expression")))
        (values
         `(cons ',tag ,value-code)
         type
         (list 'expr-annotation:special-form special-form type-expr tag value-annotated-expr type))))))


; (case <oneof-expr> (<tag-spec> <value-expr>) (<tag-spec> <value-expr>) ... (<tag-spec> <value-expr>))
; where each <tag-spec> is either <tag> or (<tag> <var> <type> [:unused]) or ((<tag> <tag> ... <tag>))
(defun scan-case (world type-env special-form oneof-expr &rest cases)
  (multiple-value-bind (oneof-code oneof-type oneof-annotated-expr) (scan-kinded-value world type-env oneof-expr :oneof)
    (let ((unseen-tags (copy-list (type-tags oneof-type)))
          (case-codes nil)
          (case-annotated-exprs nil)
          (body-type nil)
          (oneof-var (gensym "ONEOF")))
      (unless cases
        (error "Empty case statement"))
      (dolist (case cases)
        (unless (and (consp case) (= (length case) 2))
          (error "Bad case ~S" case))
        (let ((tag-spec (first case))
              (tags nil)
              (var nil)
              (var-type-expr nil)
              (local-type-env type-env))
          (cond
           ((atom tag-spec)
            (setq tags (list tag-spec)))
           ((atom (first tag-spec))
            (unless (and (consp (cdr tag-spec))
                         (consp (cddr tag-spec))
                         (member (cdddr tag-spec) '(nil (:unused)) :test #'equal))
              (error "Bad case tag ~S" tag-spec))
            (setq tags (list (first tag-spec)))
            (when (second tag-spec)
              (setq var (scan-name world (second tag-spec)))
              (setq var-type-expr (third tag-spec))))
           (t (when (rest tag-spec)
                (error "Bad case tag ~S" tag-spec))
              (setq tags (first tag-spec))))
          (dolist (tag tags)
            (multiple-value-bind (tag field-type) (scan-tag oneof-type tag)
              (if (member tag unseen-tags)
                (setq unseen-tags (delete tag unseen-tags))
                (error "Duplicate case tag ~A" tag))
              (when var
                (let ((var-type (scan-type world var-type-expr)))
                  (unless (eq field-type var-type)
                    (error "Case tag ~A type mismatch: ~A and ~S" tag
                           (print-type-to-string field-type) var-type-expr))
                  (setq local-type-env (type-env-add-bindings local-type-env (list (cons var field-type))))))))
          (multiple-value-bind (value-code value-type value-annotated-expr) (scan-value world local-type-env (second case))
            (if body-type
              (let ((new-body-type (type-lub body-type value-type)))
                (unless new-body-type
                  (error "Case result type mismatch: ~A and ~A" (print-type-to-string body-type) (print-type-to-string value-type)))
                (setq body-type new-body-type))
              (setq body-type value-type))
            (push (list tags
                        (if var
                          `(let ((,var (cdr ,oneof-var)))
                             ,@(when (eq (fourth tag-spec) ':unused)
                                 `((declare (ignore ,var))))
                             ,value-code)
                          value-code))
                  case-codes)
            (push (list (list tags var var-type-expr) value-annotated-expr) case-annotated-exprs))))
      (when unseen-tags
        (error "Missing case tags ~S" unseen-tags))
      (values
       `(let ((,oneof-var ,oneof-code))
          (ecase (car ,oneof-var) ,@(nreverse case-codes)))
       body-type
       (list* 'expr-annotation:special-form special-form oneof-annotated-expr oneof-type (nreverse case-annotated-exprs))))))


; (select <tag> <oneof-expr>)
; Returns the tag's value or bottom if <oneof-expr> has a different tag.
(defun scan-select (world type-env special-form tag oneof-expr)
  (multiple-value-bind (oneof-code oneof-type oneof-annotated-expr) (scan-kinded-value world type-env oneof-expr :oneof)
    (multiple-value-bind (tag field-type) (scan-tag oneof-type tag)
      (values
       `(select-field ',tag ,oneof-code)
       field-type
       (list 'expr-annotation:special-form special-form tag oneof-annotated-expr oneof-type)))))

(defun select-field (tag value)
  (if (eq (car value) tag)
    (cdr value)
    (error "Select ~S got tag ~S" tag (car value))))


; (is <tag> <oneof-expr>)
(defun scan-is (world type-env special-form tag oneof-expr)
  (multiple-value-bind (oneof-code oneof-type oneof-annotated-expr) (scan-kinded-value world type-env oneof-expr :oneof)
    (let ((tag (scan-tag oneof-type tag)))
      (values
       `(eq ',tag (car ,oneof-code))
       (world-boolean-type world)
       (list 'expr-annotation:special-form special-form tag oneof-annotated-expr oneof-type)))))


;;; Tuples

; (tuple <tuple-type> <field-expr1> ... <field-exprn>)
(defun scan-tuple-form (world type-env special-form type-expr &rest value-exprs)
  (let* ((type (scan-kinded-type world type-expr :tuple))
         (field-types (type-parameters type)))
    (unless (= (length value-exprs) (length field-types))
      (error "Wrong number of tuple fields given in ~A constructor: ~S" (print-type-to-string type) value-exprs))
    (multiple-value-map-bind (value-codes value-annotated-exprs)
                             #'(lambda (field-type value-expr)
                                 (scan-typed-value world type-env value-expr field-type))
                             (field-types value-exprs)
      (values
       (cons 'list value-codes)
       type
       (list* 'expr-annotation:special-form special-form type-expr type value-annotated-exprs)))))


; (& <tag> <tuple-expr>)
; Return the tuple field's value.
(defun scan-& (world type-env special-form tag tuple-expr)
  (multiple-value-bind (tuple-code tuple-type tuple-annotated-expr) (scan-kinded-value world type-env tuple-expr :tuple)
    (multiple-value-bind (tag field-type) (scan-tag tuple-type tag)
      (values
       (list 'nth (position tag (type-tags tuple-type)) tuple-code)
       field-type
       (list 'expr-annotation:special-form special-form tag tuple-annotated-expr tuple-type)))))


;;; Addresses

; (new <value-expr>)
; Makes a mutable cell with the given initial value.
(defun scan-new (world type-env special-form value-expr)
  (multiple-value-bind (value-code value-type value-annotated-expr) (scan-value world type-env value-expr)
    (values
     (let ((var (gensym "VAL")))
       `(let ((,var ,value-code))
          (cons (incf *address-counter*) ,var)))
     (make-address-type world value-type)
     (list 'expr-annotation:special-form special-form value-annotated-expr))))


; (@ <address-expr>)
; Reads the value of the mutable cell.
(defun scan-@ (world type-env special-form address-expr)
  (multiple-value-bind (address-code address-type address-annotated-expr) (scan-kinded-value world type-env address-expr :address)
    (values
     `(cdr ,address-code)
     (address-element-type address-type)
     (list 'expr-annotation:special-form special-form address-annotated-expr))))


; (@= <address-expr> <value-expr>)
; Writes the value of the mutable cell.  Returns void.
(defun scan-@= (world type-env special-form address-expr value-expr)
  (multiple-value-bind (address-code address-type address-annotated-expr) (scan-kinded-value world type-env address-expr :address)
    (multiple-value-bind (value-code value-annotated-expr) (scan-typed-value world type-env value-expr (address-element-type address-type))
      (values
       `(progn
          (rplacd ,address-code ,value-code)
          nil)
       (world-void-type world)
       (list 'expr-annotation:special-form special-form address-annotated-expr value-annotated-expr)))))


; (address-equal <address-expr1> <address-expr2>)
; Returns true if the two addresses are the same.
(defun scan-address-equal (world type-env special-form address1-expr address2-expr)
  (multiple-value-bind (address1-code address1-type address1-annotated-expr) (scan-kinded-value world type-env address1-expr :address)
    (multiple-value-bind (address2-code address2-annotated-expr) (scan-typed-value world type-env address2-expr address1-type)
      (values
       `(eq ,address1-code ,address2-code)
       (world-boolean-type world)
       (list 'expr-annotation:special-form special-form address1-annotated-expr address2-annotated-expr)))))


;;; ------------------------------------------------------------------------------------------------------
;;; MACROS


(defun let-binding? (form)
  (and (consp form)
       (consp (cdr form))
       (consp (cddr form))
       (member (cdddr form) '(nil (:unused)) :test #'equal)
       (identifier? (first form))))


; (let ((<var1> <type1> <expr1> [:unused]) ... (<varn> <typen> <exprn> [:unused])) <body>)  ==>
; ((function ((<var1> <type1> [:unused]) ... (<varn> <typen> [:unused])) <body>) <expr1> ... <exprn>)
(defun expand-let (world type-env bindings &rest body)
  (declare (ignore world type-env))
  (unless (and (listp bindings)
               (every #'let-binding? bindings))
    (error "Bad let bindings ~S" bindings))
  (cons (list* 'function (mapcar #'(lambda (binding)
                                     (list* (first binding) (second binding) (cdddr binding)))
                                 bindings) body)
        (mapcar #'third bindings)))


; (letexc (<var> <type> <expr> [:unused]) <body>)  ==>
; (case <expr>
;   ((abrupt x exception) (typed-oneof <body-type> abrupt x))
;   ((normal <var> <type> [:unused]) <body>)))
; where <body-type> is the type of <body>.
(defun expand-letexc (world type-env binding &rest body)
  (unless (let-binding? binding)
    (error "Bad letexc binding ~S" binding))
  (let* ((var (first binding))
         (type (second binding))
         (expr (third binding))
         (body-type (->-result-type (scan-value-type world type-env `(function ((,var ,type)) ,@body)))))
    `(case ,expr
       ((abrupt x exception) (typed-oneof ,body-type abrupt x))
       ((normal ,var ,type ,@(cdddr binding)) ,@body))))


; (set-of <element-type> <element-expr> ... <element-expr>)  ==>
; (set-of-ranges <element-type> <element-expr> nil ... <element-expr> nil)
(defun expand-set-of (world type-env element-type-expr &rest element-exprs)
  (declare (ignore world type-env))
  (list* 'set-of-ranges
         element-type-expr
         (mapcan #'(lambda (element-expr)
                     (list element-expr nil))
                 element-exprs)))


;;; ------------------------------------------------------------------------------------------------------
;;; COMMANDS

; (%highlight <highlight> <command> ... <command>)
; Evaluate the given commands.  <highlight> is a hint for printing.
(defun scan-%highlight (world grammar-info-var highlight &rest commands)
  (declare (ignore highlight))
  (scan-commands world grammar-info-var commands))


; (%... ...)
; Ignore any command that starts with a %.  These commands are hints for printing.
(defun scan-% (world grammar-info-var &rest rest)
  (declare (ignore world grammar-info-var rest)))


; (deftype <name> <type>)
; Create the type in the world and set its contents.
(defun scan-deftype (world grammar-info-var name type-expr)
  (declare (ignore grammar-info-var))
  (let* ((symbol (scan-name world name))
         (type (scan-type world type-expr t)))
    (unless (typep type 'type)
      (error "~:W undefined in type definition of ~A" type-expr symbol))
    (add-type-name world type symbol t)))


; (define <name> <type> <value> <destructured>)
; Create the variable in the world but do not evaluate its type or value yet.
; <destructured> is a flag that is true if this define was originally in the form:
;   (define (<name> (<arg1> <type1>) ... (<argn> <typen>)) <result-type> <value>)
(defun scan-define (world grammar-info-var name type-expr value-expr destructured)
  (declare (ignore grammar-info-var destructured))
  (let ((symbol (scan-name world name)))
    (unless (eq (get symbol :value-expr *get2-nonce*) *get2-nonce*)
      (error "Attempt to redefine variable ~A" symbol))
    (setf (get symbol :value-expr) value-expr)
    (setf (get symbol :type-expr) type-expr)
    (export-symbol symbol)))


; (set-grammar <name>)
; Set the current grammar to the grammar or lexer with the given name.
(defun scan-set-grammar (world grammar-info-var name)
  (let ((grammar-info (world-grammar-info world name)))
    (unless grammar-info
      (error "Unknown grammar ~A" name))
    (setf (car grammar-info-var) grammar-info)))


; (clear-grammar)
; Clear the current grammar.
(defun scan-clear-grammar (world grammar-info-var)
  (declare (ignore world))
  (setf (car grammar-info-var) nil))


; Get the grammar-info-var's grammar.  Signal an error if there isn't one.
(defun checked-grammar (grammar-info-var)
  (let ((grammar-info (car grammar-info-var)))
    (if grammar-info
      (grammar-info-grammar grammar-info)
      (error "Grammar needed"))))


; (declare-action <action-name> <general-grammar-symbol> <type>)
(defun scan-declare-action (world grammar-info-var action-name general-grammar-symbol-source type-expr)
  (let* ((grammar (checked-grammar grammar-info-var))
         (action-symbol (scan-name world action-name))
         (general-grammar-symbol (grammar-parametrization-intern grammar general-grammar-symbol-source)))
    (declare-action grammar general-grammar-symbol action-symbol type-expr)
    (dolist (grammar-symbol (general-grammar-symbol-instances grammar general-grammar-symbol))
      (push (cons (car grammar-info-var) grammar-symbol) (symbol-action action-symbol)))
    (export-symbol action-symbol)))


; (action <action-name> <production-name> <body> <destructured>)
; <destructured> is a flag that is true if this define was originally in the form:
;   (action (<action-name> (<arg1> <type1>) ... (<argn> <typen>)) <production-name> <body>)
(defun scan-action (world grammar-info-var action-name production-name body destructured)
  (declare (ignore destructured))
  (let ((grammar (checked-grammar grammar-info-var))
        (action-symbol (world-intern world action-name)))
    (define-action grammar production-name action-symbol body)))


; (terminal-action <action-name> <terminal> <lisp-function>)
(defun scan-terminal-action (world grammar-info-var action-name terminal function)
  (let ((grammar (checked-grammar grammar-info-var))
        (action-symbol (world-intern world action-name)))
    (define-terminal-action grammar terminal action-symbol (symbol-function function))))


;;; ------------------------------------------------------------------------------------------------------
;;; INITIALIZATION

(defparameter *default-specials*
  '((:preprocess
     (? preprocess-?)
     (define preprocess-define)
     (action preprocess-action)
     (grammar preprocess-grammar)
     (line-grammar preprocess-line-grammar)
     (lexer preprocess-lexer)
     (grammar-argument preprocess-grammar-argument)
     (production preprocess-production)
     (rule preprocess-rule)
     (exclude preprocess-exclude))
    
    (:macro
     (let expand-let depict-let)
     (letexc expand-letexc depict-letexc)
     (set-of expand-set-of nil))
    
    (:command
     (%highlight scan-%highlight depict-%highlight) ;For internal use only; use ? instead.
     (%section scan-% depict-%section)
     (%subsection scan-% depict-%subsection)
     (%text scan-% depict-%text)
     (grammar-argument scan-% depict-grammar-argument)
     (%rule scan-% depict-%rule)
     (%charclass scan-% depict-%charclass)
     (%print-actions scan-% depict-%print-actions)
     (deftype scan-deftype depict-deftype)
     (define scan-define depict-define)
     (set-grammar scan-set-grammar depict-set-grammar)
     (clear-grammar scan-clear-grammar depict-clear-grammar)
     (declare-action scan-declare-action depict-declare-action)
     (action scan-action depict-action)
     (terminal-action scan-terminal-action depict-terminal-action))
    
    (:special-form
     ;;Control structures
     (bottom scan-bottom depict-bottom)
     (function scan-function depict-function)
     (if scan-if depict-if)
     (throw scan-throw depict-throw)
     (catch scan-catch depict-catch)
     
     ;;Vectors
     (vector scan-vector-form depict-vector-form)
     (vector-of scan-vector-of depict-vector-of)
     (empty scan-empty depict-empty)
     (length scan-length depict-length)
     (nth scan-nth depict-nth)
     (subseq scan-subseq depict-subseq)
     (append scan-append depict-append)
     (set-nth scan-set-nth depict-set-nth)
     
     ;;Sets
     (set-of-ranges scan-set-of-ranges depict-set-of-ranges)
     
     ;;Oneofs
     (oneof scan-oneof-form depict-oneof-form)
     (typed-oneof scan-typed-oneof depict-typed-oneof)
     (case scan-case depict-case)
     (select scan-select depict-select-or-&)
     (is scan-is depict-is)
     
     ;;Tuples
     (tuple scan-tuple-form depict-tuple-form)
     (& scan-& depict-select-or-&)
     
     ;;Addresses
     (new scan-new depict-new)
     (@ scan-@ depict-@)
     (@= scan-@= depict-@=)
     (address-equal scan-address-equal depict-address-equal))
    
    (:type-constructor
     (-> scan--> depict-->)
     (vector scan-vector depict-vector)
     (set scan-set depict-set)
     (oneof scan-oneof depict-oneof)
     (tuple scan-tuple depict-tuple)
     (address scan-address depict-address))))


(defparameter *default-types*
  '((bottom-type . :bottom)
    (void . :void)
    (boolean . :boolean)
    (integer . :integer)
    (rational . :rational)
    (double . :double)
    (character . :character)))


(defparameter *default-primitives*
  '((empty void nil :global :empty-10 %primary%)
    (true boolean t :global :true %primary%)
    (false boolean nil :global :false %primary%)
    (+infinity double :+inf :global ("+" :infinity) %prefix%)
    (-infinity double :-inf :global (:minus :infinity) %prefix%)
    (nan double :nan :global "NaN" %primary%)
    
    (neg (-> (integer) integer) #'- :unary :minus nil %suffix% %suffix%)
    (+ (-> (integer integer) integer) #'+ :infix "+" t %term% %term% %term%)
    (- (-> (integer integer) integer) #'- :infix :minus t %term% %term% %factor%)
    (* (-> (integer integer) integer) #'* :infix "*" nil %factor% %factor% %factor%)
    (= (-> (integer integer) boolean) #'= :infix "=" t %relational% %term% %term%)
    (/= (-> (integer integer) boolean) #'/= :infix :not-equal t %relational% %term% %term%)
    (< (-> (integer integer) boolean) #'< :infix "<" t %relational% %term% %term%)
    (> (-> (integer integer) boolean) #'> :infix ">" t %relational% %term% %term%)
    (<= (-> (integer integer) boolean) #'<= :infix :less-or-equal t %relational% %term% %term%)
    (>= (-> (integer integer) boolean) #'>= :infix :greater-or-equal t %relational% %term% %term%)
    
    (rational-neg (-> (rational) rational) #'- :unary "-" nil %suffix% %suffix%)
    (rational+ (-> (rational rational) rational) #'+ :infix "+" t %term% %term% %term%)
    (rational- (-> (rational rational) rational) #'- :infix :minus t %term% %term% %factor%)
    (rational* (-> (rational rational) rational) #'* :infix "*" nil %factor% %factor% %factor%)
    (rational/ (-> (rational rational) rational) #'/ :infix "/" nil %factor% %factor% %unary%)
    
    (not (-> (boolean) boolean) #'not :unary ((:semantic-keyword "not") " ") nil %not% %not%)
    (and (-> (boolean boolean) boolean) #'and2 :infix ((:semantic-keyword "and")) t %and% %and% %and%)
    (or (-> (boolean boolean) boolean) #'or2 :infix ((:semantic-keyword "or")) t %or% %or% %or%)
    (xor (-> (boolean boolean) boolean) #'xor2 :infix ((:semantic-keyword "xor")) t %xor% %xor% %xor%)
    
    (bitwise-and (-> (integer integer) integer) #'logand)
    (bitwise-or (-> (integer integer) integer) #'logior)
    (bitwise-xor (-> (integer integer) integer) #'logxor)
    (bitwise-shift (-> (integer integer) integer) #'ash)
    
    (rational-to-double (-> (rational) double) #'rational-to-double)
    
    (double-is-zero (-> (double) boolean) #'double-is-zero)
    (double-is-nan (-> (double) boolean) #'double-is-nan)
    (double-compare (-> (double double boolean boolean boolean boolean) boolean) #'double-compare)
    (double-to-uint32 (-> (double) integer) #'double-to-uint32)
    (double-abs (-> (double double) double) #'double-abs)
    (double-negate (-> (double) double) #'double-neg)
    (double-add (-> (double double) double) #'double-add)
    (double-subtract (-> (double double) double) #'double-subtract)
    (double-multiply (-> (double double) double) #'double-multiply)
    (double-divide (-> (double double) double) #'double-divide)
    (double-remainder (-> (double double) double) #'double-remainder)
    
    (code-to-character (-> (integer) character) #'code-char)
    (character-to-code (-> (character) integer) #'char-code)
    
    (char= (-> (character character) boolean) #'char= :infix "=" t %relational% %term% %term%)
    (char/= (-> (character character) boolean) #'char/= :infix :not-equal t %relational% %term% %term%)
    (char< (-> (character character) boolean) #'char< :infix "<" t %relational% %term% %term%)
    (char> (-> (character character) boolean) #'char> :infix ">" t %relational% %term% %term%)
    (char<= (-> (character character) boolean) #'char<= :infix :less-or-equal t %relational% %term% %term%)
    (char>= (-> (character character) boolean) #'char>= :infix :greater-or-equal t %relational% %term% %term%)
    
    (string-equal (-> (string string) boolean) #'string= :infix "=" t %relational% %term% %term%)
    
    (integer-set-length (-> (integer-set) integer) #'intset-length :unary "|" "|" %primary% %expr%)
    (integer-set-min (-> (integer-set) integer) #'integer-set-min :unary ((:semantic-keyword "min") " ") nil %min-max% %prefix%)
    (integer-set-max (-> (integer-set) integer) #'integer-set-max :unary ((:semantic-keyword "max") " ") nil %min-max% %prefix%)
    (integer-set-intersection (-> (integer-set integer-set) integer-set) #'intset-intersection :infix :intersection-10 t %factor% %factor% %factor%)
    (integer-set-union (-> (integer-set integer-set) integer-set) #'intset-union :infix :union-10 t %term% %term% %term%)
    (integer-set-difference (-> (integer-set integer-set) integer-set) #'intset-difference :infix :minus t %term% %term% %factor%)
    (integer-set-member (-> (integer integer-set) boolean) #'integer-set-member :infix :member-10 t %relational% %term% %term%)
    (integer-set= (-> (integer-set integer-set) boolean) #'intset= :infix "=" t %relational% %term% %term%)
    
    (character-set-length (-> (character-set) integer) #'intset-length :unary "|" "|" %primary% %expr%)
    (character-set-min (-> (character-set) character) #'character-set-min :unary ((:semantic-keyword "min") " ") nil %min-max% %prefix%)
    (character-set-max (-> (character-set) character) #'character-set-max :unary ((:semantic-keyword "max") " ") nil %min-max% %prefix%)
    (character-set-intersection (-> (character-set character-set) character-set) #'intset-intersection :infix :intersection-10 t %factor% %factor% %factor%)
    (character-set-union (-> (character-set character-set) character-set) #'intset-union :infix :union-10 t %term% %term% %term%)
    (character-set-difference (-> (character-set character-set) character-set) #'intset-difference :infix :minus t %term% %term% %factor%)
    (character-set-member (-> (character character-set) boolean) #'character-set-member :infix :member-10 t %relational% %term% %term%)
    (character-set= (-> (character-set character-set) boolean) #'intset= :infix "=" t %relational% %term% %term%)
    
    (digit-value (-> (character) integer) #'digit-char-36)
    (is-ordinary-initial-identifier-character (-> (character) boolean) #'ordinary-initial-identifier-character?)
    (is-ordinary-continuing-identifier-character (-> (character) boolean) #'ordinary-continuing-identifier-character?)))


;;; Partial order of primitives for deciding when to depict parentheses.
(defparameter *primitive-level* (make-partial-order))
(def-partial-order-element *primitive-level* %primary%)                                          ;id, constant, (e), |e|, action
(def-partial-order-element *primitive-level* %suffix% %primary%)                                 ;f(...), new(v), a[i], a[i...j], a[i<-v], a.f
(def-partial-order-element *primitive-level* %prefix% %primary%)                                 ;-e, @, oneof-tag val
(def-partial-order-element *primitive-level* %min-max% %prefix%)                                 ;min, max
(def-partial-order-element *primitive-level* %unary% %suffix% %prefix%)                          ;
(def-partial-order-element *primitive-level* %factor% %unary%)                                   ;/, *, intersection
(def-partial-order-element *primitive-level* %term% %factor%)                                    ;+, -, append, union, set difference
(def-partial-order-element *primitive-level* %relational% %term% %min-max%)                      ;<, <=, >, >=, =, /=, address=, is, member
(def-partial-order-element *primitive-level* %not% %relational%)                                 ;not
(def-partial-order-element *primitive-level* %and% %not%)                                        ;and
(def-partial-order-element *primitive-level* %or% %not%)                                         ;or
(def-partial-order-element *primitive-level* %xor% %not%)                                        ;xor
(def-partial-order-element *primitive-level* %logical% %and% %or% %xor%)                         ;
(def-partial-order-element *primitive-level* %unparenthesized-new% %logical%)                    ;new v
(defparameter %expr% %unparenthesized-new%)
(def-partial-order-element *primitive-level* %stmt% %expr%)                                      ;:=, function, if/then/else
(defparameter %max% %stmt%)


; Return the tail end of the lambda list for make-primitive.  The returned list always starts with
; an appearance constant and is followed by additional keywords as appropriate for that appearance.
(defun process-primitive-spec-appearance (name primitive-spec-appearance)
  (if primitive-spec-appearance
    (let ((appearance (first primitive-spec-appearance))
          (args (rest primitive-spec-appearance)))
      (cons
       appearance
       (ecase appearance
         (:global
          (assert-type args (tuple t symbol))
          (list ':markup1 (first args) ':level (symbol-value (second args))))
         (:infix
          (assert-type args (tuple t bool symbol symbol symbol))
          (list ':markup1 (first args) ':markup2 (second args) ':level (symbol-value (third args))
                ':level1 (symbol-value (fourth args)) ':level2 (symbol-value (fifth args))))
         (:unary
          (assert-type args (tuple t t symbol symbol))
          (list ':markup1 (first args) ':markup2 (second args) ':level (symbol-value (third args))
                ':level1 (symbol-value (fourth args))))
         (:phantom
          (assert-true (null args))
          (list ':level %primary%)))))
    (let ((name (symbol-lower-mixed-case-name name)))
      `(:global :markup1 ((:global-variable ,name)) :markup2 ,name :level ,%primary%))))


; Create a world with the given name and set up the built-in properties of its symbols.
; conditionals is an association list of (conditional . highlight), where conditional is a symbol
; and highlight is either:
;   a style keyword:   Use that style to highlight the contents of any (? conditional ...) commands
;   nil:               Include the contents of any (? conditional ...) commands without highlighting them
;   delete:            Don't include the contents of (? conditional ...) commands
(defun init-world (name conditionals)
  (assert-type conditionals (list (cons symbol (or null keyword (eql delete)))))
  (let ((world (make-world name)))
    (setf (world-conditionals world) conditionals)
    (dolist (specials-list *default-specials*)
      (let ((property (car specials-list)))
        (dolist (special-spec (cdr specials-list))
          (apply #'add-special
            property
            (world-intern world (first special-spec))
            (rest special-spec)))))
    (dolist (primitive-spec *default-primitives*)
      (let ((name (world-intern world (first primitive-spec))))
        (apply #'declare-primitive
          name
          (second primitive-spec)
          (third primitive-spec)
          (process-primitive-spec-appearance name (cdddr primitive-spec)))))
    (dolist (type-spec *default-types*)
      (add-type-name world (make-type world (cdr type-spec) nil nil) (world-intern world (car type-spec)) nil))
    (add-type-name world (make-vector-type world (make-type world :character nil nil)) (world-intern world 'string) nil)
    (add-type-name world (make-set-type world (make-type world :integer nil nil)) (world-intern world 'integer-set) nil)
    (add-type-name world (make-set-type world (make-type world :character nil nil)) (world-intern world 'character-set) nil)
    world))


(defun print-world (world &optional (stream t) (all t))
  (pprint-logical-block (stream nil)
    (labels
      ((default-print-contents (symbol value stream)
         (declare (ignore symbol))
         (write value :stream stream))
       
       (print-symbols-and-contents (property title separator print-contents)
         (let ((symbols (all-world-external-symbols-with-property world property)))
           (when symbols
             (pprint-logical-block (stream symbols)
               (write-string title stream)
               (pprint-indent :block 2 stream)
               (pprint-newline :mandatory stream)
               (loop
                 (let ((symbol (pprint-pop)))
                   (pprint-logical-block (stream nil)
                     (if separator
                       (format stream "~A ~@_~:I~A " symbol separator)
                       (format stream "~A " symbol))
                     (funcall print-contents symbol (get symbol property) stream)))
                 (pprint-exit-if-list-exhausted)
                 (pprint-newline :mandatory stream)))
             (pprint-newline :mandatory stream)
             (pprint-newline :mandatory stream)))))
      
      (when all
        (print-symbols-and-contents
         :preprocess "Preprocessor actions:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :command "Commands:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :special-form "Special Forms:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :primitive "Primitives:" ":"
         #'(lambda (symbol primitive stream)
             (declare (ignore symbol))
             (let ((type (primitive-type primitive)))
               (if type
                 (print-type type stream)
                 (format stream "~@<<<~;~W~;>>~:>" (primitive-type-expr primitive))))
             (format stream " ~_= ~@<<~;~W~;>~:>" (primitive-value-code primitive))))
        (print-symbols-and-contents
         :macro "Macros:" "::" #'default-print-contents)
        (print-symbols-and-contents
         :type-constructor "Type Constructors:" "::" #'default-print-contents))
      
      (print-symbols-and-contents
       :deftype "Types:" "=="
       #'(lambda (symbol type stream)
           (if type
             (print-type type stream (eq symbol (type-name type)))
             (format stream "<forward-referenced>"))))
      (print-symbols-and-contents
       :value-expr "Values:" ":"
       #'(lambda (symbol value-expr stream)
           (let ((type (symbol-type symbol)))
             (if type
               (print-type type stream)
               (format stream "~@<<<~;~W~;>>~:>" (get symbol :type-expr)))
             (format stream " ~_= ")
             (if (boundp symbol)
               (print-value (symbol-value symbol) type stream)
               (format stream "~@<<<~;~W~;>>~:>" value-expr)))))
      (print-symbols-and-contents
       :action "Actions:" nil
       #'(lambda (action-symbol grammar-info-and-symbols stream)
           (pprint-newline :miser stream)
           (pprint-logical-block (stream (reverse grammar-info-and-symbols))
             (pprint-exit-if-list-exhausted)
             (loop
               (let* ((grammar-info-and-symbol (pprint-pop))
                      (grammar-info (car grammar-info-and-symbol))
                      (grammar (grammar-info-grammar grammar-info))
                      (grammar-symbol (cdr grammar-info-and-symbol)))
                 (write-string ": " stream)
                 (multiple-value-bind (has-type type) (action-declaration grammar grammar-symbol action-symbol)
                   (declare (ignore has-type))
                   (pprint-logical-block (stream nil)
                     (print-type type stream)
                     (format stream " ~_{~S ~S}" (grammar-info-name grammar-info) grammar-symbol))))
               (pprint-exit-if-list-exhausted)
               (pprint-newline :mandatory stream))))))))


(defmethod print-object ((world world) stream)
  (print-unreadable-object (world stream)
    (format stream "world ~A" (world-name world))))


;;; ------------------------------------------------------------------------------------------------------
;;; EVALUATION

; Scan a command.  Create types and variables in the world but do not evaluate variables' types or values yet.
; grammar-info-var is a cons cell whose car is either nil or a grammar-info for the grammar currently being defined.
(defun scan-command (world grammar-info-var command)
  (handler-bind ((error #'(lambda (condition)
                            (declare (ignore condition))
                            (format *error-output* "~&~@<~2IWhile processing: ~_~:W~:>~%" command))))
    (let ((handler (and (consp command)
                        (identifier? (first command))
                        (get (world-intern world (first command)) :command))))
      (if handler
        (apply handler world grammar-info-var (rest command))
        (error "Bad command")))))


; Scan a list of commands.  See scan-command above.
(defun scan-commands (world grammar-info-var commands)
  (dolist (command commands)
    (scan-command world grammar-info-var command)))


; Compute the primitives' types from their type-exprs.
(defun define-primitives (world)
  (each-world-external-symbol-with-property
   world
   :primitive
   #'(lambda (symbol primitive)
       (declare (ignore symbol))
       (define-primitive world primitive))))


; Compute the types and values of all variables accumulated by scan-command.
(defun eval-variables (world)
  ;Compute the variables' types first.
  (each-world-external-symbol-with-property
   world
   :type-expr
   #'(lambda (symbol type-expr)
       (setf (get symbol :type) (scan-type world type-expr))))
  
  ;Then compute the variables' values.
  (each-world-external-symbol-with-property
   world
   :value-expr
   #'(lambda (symbol value-expr)
       (declare (ignore value-expr))
       (compute-variable-value symbol))))


; Compute the types of all grammar declarations accumulated by scan-declare-action.
(defun eval-action-declarations (world)
  (dolist (grammar (world-grammars world))
    (each-action-declaration
     grammar
     #'(lambda (grammar-symbol action-declaration)
         (declare (ignore grammar-symbol))
         (setf (cdr action-declaration) (scan-type world (cdr action-declaration)))))))


; Compute the bodies of all grammar actions accumulated by scan-action.
(defun eval-action-definitions (world)
  (dolist (grammar (world-grammars world))
    (maphash
     #'(lambda (terminal action-bindings)
         (dolist (action-binding action-bindings)
           (unless (cdr action-binding)
             (error "Missing action ~S for terminal ~S" (car action-binding) terminal))))
     (grammar-terminal-actions grammar))
    (each-grammar-production
     grammar
     #'(lambda (production)
         (let* ((n-action-args (n-action-args grammar production))
                (codes
                 (mapcar
                  #'(lambda (action-binding)
                      (let ((action-symbol (car action-binding))
                            (action (cdr action-binding)))
                        (unless action
                          (error "Missing action ~S for production ~S" (car action-binding) (production-name production)))
                        (multiple-value-bind (has-type type) (action-declaration grammar (production-lhs production) action-symbol)
                          (declare (ignore has-type))
                          (let ((code (compute-action-code world grammar production action-symbol (action-expr action) type)))
                            (setf (action-code action) code)
                            (when *trace-variables*
                              (format *trace-output* "~&~@<~S[~S] := ~2I~_~:W~:>~%" action-symbol (production-name production) code))
                            code))))
                  (production-actions production)))
                (production-code
                 (if codes
                   (let* ((vars-and-rest (intern-n-vars-with-prefix "ARG" n-action-args '(stack-rest)))
                          (vars (nreverse (butlast vars-and-rest)))
                          (applied-codes (mapcar #'(lambda (code) (apply #'gen-apply code vars))
                                                 (nreverse codes))))
                     `#'(lambda (stack)
                          (list*-bind ,vars-and-rest stack
                            (list* ,@applied-codes stack-rest))))
                   `#'(lambda (stack)
                        (nthcdr ,n-action-args stack))))
                (named-production-code (name-lambda production-code (production-name production))))
           (setf (production-n-action-args production) n-action-args)
           (setf (production-evaluator-code production) named-production-code)
           (when *trace-variables*
             (format *trace-output* "~&~@<all[~S] := ~2I~_~:W~:>~%" (production-name production) named-production-code))
           (handler-bind ((warning #'(lambda (condition)
                                       (declare (ignore condition))
                                       (format *error-output* "~&While computing production ~S:~%" (production-name production)))))
             (setf (production-evaluator production) (eval named-production-code))))))))


; Evaluate the given commands in the world.
; This method can only be called once.
(defun eval-commands (world commands)
  (ensure-proper-form commands)
  (assert-true (null (world-commands-source world)))
  (setf (world-commands-source world) commands)
  (let ((grammar-info-var (list nil)))
    (scan-commands world grammar-info-var commands))
  (unite-types world)
  (setf (world-bottom-type world) (make-type world :bottom nil nil))
  (setf (world-void-type world) (make-type world :void nil nil))
  (setf (world-boolean-type world) (make-type world :boolean nil nil))
  (setf (world-integer-type world) (make-type world :integer nil nil))
  (setf (world-rational-type world) (make-type world :rational nil nil))
  (setf (world-double-type world) (make-type world :double nil nil))
  (setf (world-character-type world) (make-type world :character nil nil))
  (setf (world-string-type world) (make-vector-type world (world-character-type world)))
  (define-primitives world)
  (eval-action-declarations world)
  (eval-variables world)
  (eval-action-definitions world))


;;; ------------------------------------------------------------------------------------------------------
;;; PREPROCESSING

(defstruct (preprocessor-state (:constructor make-preprocessor-state (world)))
  (world nil :type world :read-only t)                ;The world into which preprocessed symbols are interned
  (highlight nil :type symbol)                        ;The current highlight style or nil if none
  (kind nil :type (member nil :grammar :lexer))       ;The kind of grammar being accumulated or nil if none
  (kind2 nil :type (member nil :lalr-1 :lr-1 :canonical-lr-1)) ;The kind of parser
  (name nil :type symbol)                             ;Name of the grammar being accumulated or nil if none
  (parametrization nil :type (or null grammar-parametrization)) ;Parametrization of the grammar being accumulated or nil if none
  (start-symbol nil :type symbol)                     ;Start symbol of the grammar being accumulated or nil if none
  (grammar-source-reverse nil :type list)             ;List of productions in the grammar being accumulated (in reverse order)
  (excluded-nonterminals-source nil :type list)       ;List of nonterminals to be excluded from the grammar
  (grammar-options nil :type list)                    ;List of other options for make-grammar
  (charclasses-source nil)                            ;List of charclasses in the lexical grammar being accumulated
  (lexer-actions-source nil)                          ;List of lexer actions in the lexical grammar being accumulated
  (grammar-infos-reverse nil :type list))             ;List of grammar-infos already completed (in reverse order)


; Ensure that the preprocessor-state is accumulating a grammar or a lexer.
(defun preprocess-ensure-grammar (preprocessor-state)
  (unless (preprocessor-state-kind preprocessor-state)
    (error "No active grammar at this point")))


; Finish generating the current grammar-info if one is in progress.
; Return any extra commands needed for this grammar-info.
; The result list can be mutated using nconc.
(defun preprocessor-state-finish-grammar (preprocessor-state)
  (let ((kind (preprocessor-state-kind preprocessor-state)))
    (and kind
         (let ((parametrization (preprocessor-state-parametrization preprocessor-state))
               (start-symbol (preprocessor-state-start-symbol preprocessor-state))
               (grammar-source (nreverse (preprocessor-state-grammar-source-reverse preprocessor-state)))
               (excluded-nonterminals-source (preprocessor-state-excluded-nonterminals-source preprocessor-state))
               (grammar-options (preprocessor-state-grammar-options preprocessor-state))
               (highlights (world-highlights (preprocessor-state-world preprocessor-state))))
           (multiple-value-bind (grammar lexer extra-commands)
                                (ecase kind
                                  (:grammar
                                   (values (apply #'make-and-compile-grammar
                                             (preprocessor-state-kind2 preprocessor-state)
                                             parametrization
                                             start-symbol
                                             grammar-source
                                             :excluded-nonterminals excluded-nonterminals-source
                                             :highlights highlights
                                             grammar-options)
                                           nil
                                           nil))
                                  (:lexer 
                                   (multiple-value-bind (lexer extra-commands)
                                                        (apply #'make-lexer-and-grammar
                                                          (preprocessor-state-kind2 preprocessor-state)
                                                          (preprocessor-state-charclasses-source preprocessor-state)
                                                          (preprocessor-state-lexer-actions-source preprocessor-state)
                                                          parametrization
                                                          start-symbol
                                                          grammar-source
                                                          :excluded-nonterminals excluded-nonterminals-source
                                                          :highlights highlights
                                                          grammar-options)
                                     (values (lexer-grammar lexer) lexer extra-commands))))
             (let ((grammar-info (make-grammar-info (preprocessor-state-name preprocessor-state) grammar lexer)))
               (setf (preprocessor-state-kind preprocessor-state) nil)
               (setf (preprocessor-state-kind2 preprocessor-state) nil)
               (setf (preprocessor-state-name preprocessor-state) nil)
               (setf (preprocessor-state-parametrization preprocessor-state) nil)
               (setf (preprocessor-state-start-symbol preprocessor-state) nil)
               (setf (preprocessor-state-grammar-source-reverse preprocessor-state) nil)
               (setf (preprocessor-state-excluded-nonterminals-source preprocessor-state) nil)
               (setf (preprocessor-state-grammar-options preprocessor-state) nil)
               (setf (preprocessor-state-charclasses-source preprocessor-state) nil)
               (setf (preprocessor-state-lexer-actions-source preprocessor-state) nil)
               (push grammar-info (preprocessor-state-grammar-infos-reverse preprocessor-state))
               (append extra-commands (list '(clear-grammar)))))))))


; Helper function for preprocess-source.
; source is a list of preprocessor directives and commands.  Preprocess these commands
; using the given preprocessor-state and return the resulting list of commands.
(defun preprocess-list (preprocessor-state source)
  (let ((world (preprocessor-state-world preprocessor-state)))
    (flet
      ((preprocess-one (form)
         (when (consp form)
           (let ((first (car form)))
             (when (identifier? first)
               (let ((action (symbol-preprocessor-function (world-intern world first))))
                 (when action
                   (handler-bind ((error #'(lambda (condition)
                                             (declare (ignore condition))
                                             (format *error-output* "~&~@<~2IWhile preprocessing: ~_~:W~:>~%" form))))
                     (multiple-value-bind (preprocessed-form re-preprocess) (apply action preprocessor-state form)
                       (return-from preprocess-one
                         (if re-preprocess
                           (preprocess-list preprocessor-state preprocessed-form)
                           preprocessed-form)))))))))
         (list form)))
      
      (mapcan #'preprocess-one source))))


; source is a list of preprocessor directives and commands.  Preprocess these commands
; and return the following results:
;   a list of preprocessed commands;
;   a list of grammar-infos extracted from preprocessor directives.
(defun preprocess-source (world source)
  (let* ((preprocessor-state (make-preprocessor-state world))
         (commands (preprocess-list preprocessor-state source))
         (commands (nconc commands (preprocessor-state-finish-grammar preprocessor-state))))
    (values commands (nreverse (preprocessor-state-grammar-infos-reverse preprocessor-state)))))


; Create a new world with the given name and preprocess and evaluate the given
; source commands in it.
; conditionals is an association list of (conditional . highlight), where conditional is a symbol
; and highlight is either:
;   a style keyword:   Use that style to highlight the contents of any (? conditional ...) commands
;   nil:               Include the contents of any (? conditional ...) commands without highlighting them
;   delete:            Don't include the contents of (? conditional ...) commands
(defun generate-world (name source &optional conditionals)
  (let ((world (init-world name conditionals)))
    (multiple-value-bind (commands grammar-infos) (preprocess-source world source)
      (dolist (grammar-info grammar-infos)
        (clear-actions (grammar-info-grammar grammar-info)))
      (setf (world-grammar-infos world) grammar-infos)
      (eval-commands world commands)
      world)))


;;; ------------------------------------------------------------------------------------------------------
;;; PREPROCESSOR ACTIONS


; (? <conditional> <command> ... <command>)
;   ==>
; (%highlight <highlight> <command> ... <command>)
;   or
; <empty>
(defun preprocess-? (preprocessor-state command conditional &rest commands)
  (declare (ignore command))
  (let ((highlight (resolve-conditional (preprocessor-state-world preprocessor-state) conditional))
        (saved-highlight (preprocessor-state-highlight preprocessor-state)))
    (cond
     ((eq highlight 'delete) (values nil nil))
     ((eq highlight saved-highlight) (values commands t))
     (t (values
         (unwind-protect
           (progn
             (setf (preprocessor-state-highlight preprocessor-state) highlight)
             (list (list* '%highlight highlight (preprocess-list preprocessor-state commands))))
           (setf (preprocessor-state-highlight preprocessor-state) saved-highlight))
         nil)))))


; (define <name> <type> <value>)
;   ==>
; (define <name> <type> <value> nil)
;
; (define (<name> (<arg1> <type1>) ... (<argn> <typen>)) <result-type> <value>)
;   ==>
; (define <name> (-> (<type1> ... <typen>) <result-type>)
;    (function ((<arg1> <type1>) ... (<argn> <typen>)) <value>)
;    t)
(defun preprocess-define (preprocessor-state command name type value)
  (declare (ignore preprocessor-state))
  (values (list
           (if (consp name)
             (let ((name (first name))
                   (bindings (rest name)))
               (list command
                     name
                     (list '-> (mapcar #'second bindings) type)
                     (list 'function bindings value)
                     t))
             (list command name type value nil)))
          nil))


; (action <action-name> <production-name> <body>)
;   ==>
; (action <action-name> <production-name> <body> nil)
;
; (action (<action-name> (<arg1> <type1>) ... (<argn> <typen>)) <production-name> <body>)
;   ==>
; (action <action-name> <production-name> (function ((<arg1> <type1>) ... (<argn> <typen>)) <body>) t)
(defun preprocess-action (preprocessor-state command action-name production-name body)
  (declare (ignore preprocessor-state))
  (values (list
           (if (consp action-name)
             (let ((action-name (first action-name))
                   (bindings (rest action-name)))
               (list command
                     action-name
                     production-name
                     (list 'function bindings body)
                     t))
             (list command action-name production-name body nil)))
          nil))


(defun preprocess-grammar-or-lexer (preprocessor-state kind kind2 name start-symbol &rest grammar-options)
  (assert-type name identifier)
  (let ((commands (preprocessor-state-finish-grammar preprocessor-state)))
    (when (find name (preprocessor-state-grammar-infos-reverse preprocessor-state) :key #'grammar-info-name)
      (error "Duplicate grammar ~S" name))
    (setf (preprocessor-state-kind preprocessor-state) kind)
    (setf (preprocessor-state-kind2 preprocessor-state) kind2)
    (setf (preprocessor-state-name preprocessor-state) name)
    (setf (preprocessor-state-parametrization preprocessor-state) (make-grammar-parametrization))
    (setf (preprocessor-state-start-symbol preprocessor-state) start-symbol)
    (setf (preprocessor-state-grammar-options preprocessor-state) grammar-options)
    (values
     (nconc commands (list (list 'set-grammar name)))
     nil)))


; (grammar <name> <kind> <start-symbol>)
;   ==>
; grammar:
;   Begin accumulating a grammar with the given name and start symbol;
; commands:
;   (set-grammar <name>)
(defun preprocess-grammar (preprocessor-state command name kind2 start-symbol)
  (declare (ignore command))
  (preprocess-grammar-or-lexer preprocessor-state :grammar kind2 name start-symbol))


(defun generate-line-break-constraints (terminal)
  (assert-type terminal user-terminal)
  (list 
   (list terminal :line-break)
   (list (make-lf-terminal terminal) :no-line-break)))


; (line-grammar <name> <kind> <start-symbol>)
;   ==>
; grammar:
;   Begin accumulating a grammar with the given name and start symbol.
;   Allow :no-line-break constraints.
; commands:
;   (set-grammar <name>)
(defun preprocess-line-grammar (preprocessor-state command name kind2 start-symbol)
  (declare (ignore command))
  (preprocess-grammar-or-lexer preprocessor-state :grammar kind2 name start-symbol
                               :variant-constraint-names '(:line-break :no-line-break)
                               :variant-generator #'generate-line-break-constraints))


; (lexer <name> <kind> <start-symbol> <charclasses-source> <lexer-actions-source>)
;   ==>
; grammar:
;   Begin accumulating a lexer with the given name, start symbol, charclasses, and lexer actions;
; commands:
;   (set-grammar <name>)
(defun preprocess-lexer (preprocessor-state command name kind2 start-symbol charclasses-source lexer-actions-source)
  (declare (ignore command))
  (multiple-value-prog1
    (preprocess-grammar-or-lexer preprocessor-state :lexer kind2 name start-symbol)
    (setf (preprocessor-state-charclasses-source preprocessor-state) charclasses-source)
    (setf (preprocessor-state-lexer-actions-source preprocessor-state) lexer-actions-source)))


; (grammar-argument <argument> <attribute> <attribute> ... <attribute>)
;   ==>
; grammar parametrization:
;   (<argument> <attribute> <attribute> ... <attribute>)
; commands:
;   (grammar-argument <argument> <attribute> <attribute> ... <attribute>)
(defun preprocess-grammar-argument (preprocessor-state command argument &rest attributes)
  (preprocess-ensure-grammar preprocessor-state)
  (grammar-parametrization-declare-argument (preprocessor-state-parametrization preprocessor-state) argument attributes)
  (values (list (list* command argument attributes))
          nil))


; (production <lhs> <rhs> <name> (<action-spec-1> <body-1>) ... (<action-spec-n> <body-n>))
;   ==>
; grammar:
;   (<lhs> <rhs> <name> <current-highlight>)
; commands:
;   (%rule <lhs>)
;   (action <action-spec-1> <name> <body-1>)
;   ...
;   (action <action-spec-n> <name> <body-n>)
(defun preprocess-production (preprocessor-state command lhs rhs name &rest actions)
  (declare (ignore command))
  (assert-type actions (list (tuple t t)))
  (preprocess-ensure-grammar preprocessor-state)
  (push (list lhs rhs name (preprocessor-state-highlight preprocessor-state))
        (preprocessor-state-grammar-source-reverse preprocessor-state))
  (values
   (cons (list '%rule lhs)
         (mapcar #'(lambda (action)
                     (list 'action (first action) name (second action)))
                 actions))
   t))


; (rule <general-grammar-symbol>
;       ((<action-name-1> <type-1>) ... (<action-name-n> <type-n>))
;   (production <lhs-1> <rhs-1> <name-1> (<action-spec-1-1> <body-1-1>) ... (<action-spec-1-n> <body-1-n>))
;   ...
;   (production <lhs-m> <rhs-m> <name-m> (<action-spec-m-1> <body-m-1>) ... (<action-spec-m-n> <body-m-n>)))
;   ==>
; grammar:
;   (<lhs-1> <rhs-1> <name-1> <current-highlight>)
;   ...
;   (<lhs-m> <rhs-m> <name-m> <current-highlight>)
; commands:
;   (%rule <lhs-1>)
;   ...
;   (%rule <lhs-m>)
;   (declare-action <action-name-1> <general-grammar-symbol> <type-1>)
;      (action <action-spec-1-1> <name-1> <body-1-1>)
;      ...
;      (action <action-spec-m-1> <name-m> <body-m-1>)
;   ...
;   (declare-action <action-name-n> <general-grammar-symbol> <type-n>)
;      (action <action-spec-1-n> <name-1> <body-1-n>)
;      ...
;      (action <action-spec-m-n> <name-m> <body-m-n>)
(defun preprocess-rule (preprocessor-state command general-grammar-symbol action-declarations &rest productions)
  (declare (ignore command))
  (assert-type action-declarations (list (tuple symbol t)))
  (preprocess-ensure-grammar preprocessor-state)
  (labels
    ((actions-match (action-declarations actions)
       (or (and (endp action-declarations) (endp actions))
           (let ((declared-action-name (caar action-declarations))
                 (action-name (caar actions)))
             (when (consp action-name)
               (setq action-name (first action-name)))
             (and (eq declared-action-name action-name)
                  (actions-match (rest action-declarations) (rest actions)))))))
    
    (let ((commands-reverse nil))
      (dolist (production productions)
        (assert-true (eq (first production) 'production))
        (let ((lhs (second production))
              (rhs (third production))
              (name (assert-type (fourth production) symbol))
              (actions (assert-type (cddddr production) (list (tuple t t)))))
          (unless (actions-match action-declarations actions)
            (error "Action name mismatch: ~S vs. ~S" action-declarations actions))
          (push (list lhs rhs name (preprocessor-state-highlight preprocessor-state))
                (preprocessor-state-grammar-source-reverse preprocessor-state))
          (push (list '%rule lhs) commands-reverse)))
      (dotimes (i (length action-declarations))
        (let ((action-declaration (nth i action-declarations)))
          (push (list 'declare-action (first action-declaration) general-grammar-symbol (second action-declaration)) commands-reverse)
          (dolist (production productions)
            (let ((name (fourth production))
                  (action (nth (+ i 4) production)))
              (push (list 'action (first action) name (second action)) commands-reverse)))))
      (values (nreverse commands-reverse) t))))


; (exclude <lhs> ... <lhs>)
;   ==>
; grammar excluded nonterminals:
;   <lhs> ... <lhs>;
(defun preprocess-exclude (preprocessor-state command &rest excluded-nonterminals-source)
  (declare (ignore command))
  (preprocess-ensure-grammar preprocessor-state)
  (setf (preprocessor-state-excluded-nonterminals-source preprocessor-state)
        (append excluded-nonterminals-source (preprocessor-state-excluded-nonterminals-source preprocessor-state)))
  (values nil nil))

