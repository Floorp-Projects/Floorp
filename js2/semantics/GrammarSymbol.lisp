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
;;; LALR(1) and LR(1) parametrized grammar utilities
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


;;; ------------------------------------------------------------------------------------------------------
;;; UTILITIES

(declaim (inline identifier?))
(defun identifier? (form)
  (and form (symbolp form) (not (keywordp form))))

(deftype identifier () '(satisfies identifier?))


; Make sure that form is one of the following:
;   A symbol
;   An integer
;   A float
;   A character
;   A string
;   A list of zero or more forms that also satisfy ensure-proper-form;
;     the list cannot be dotted.
; Return the form.
(defun ensure-proper-form (form)
  (labels
    ((ensure-list-form (form)
       (or (null form)
           (and (consp form)
                (progn
                  (ensure-proper-form (car form))
                  (ensure-list-form (cdr form)))))))
    (unless
      (or (symbolp form)
          (integerp form)
          (floatp form)
          (characterp form)
          (stringp form)
          (ensure-list-form form))
      (error "Bad form: ~S" form))
    form))


;;; ------------------------------------------------------------------------------------------------------
;;; TERMINALS

; A terminal is any of the following:
;   A symbol that is neither nil nor a keyword
;   A string;
;   A character;
;   An integer.
(defun terminal? (x)
  (and x
       (or (and (symbolp x) (not (keywordp x)))
           (stringp x)
           (characterp x)
           (integerp x))))

; The following terminals are reserved and may not be used in user input:
;   $$   Marker for end of token stream
(defconstant *end-marker* '$$)
(defconstant *end-marker-terminal-number* 0)

(deftype terminal () '(satisfies terminal?))
(deftype user-terminal () `(and terminal (not (eql ,*end-marker*))))


; Emit markup for a terminal.  subscript is an optional integer.
(defun depict-terminal (markup-stream terminal &optional subscript)
  (cond
   ((characterp terminal)
    (depict-char-style (markup-stream :character-literal)
      (depict-character markup-stream terminal)))
   ((and terminal (symbolp terminal))
    (let ((name (symbol-name terminal)))
      (if (and (> (length name) 0) (char= (char name 0) #\$))
        (depict-char-style (markup-stream :terminal)
          (depict markup-stream (subseq (symbol-upper-mixed-case-name terminal) 1)))
        (depict-char-style (markup-stream :terminal-keyword)
          (depict markup-stream (string-downcase name))))))
   (t (error "Don't know how to emit markup for terminal ~S" terminal)))
  (when subscript
    (depict-char-style (markup-stream :subscript)
      (depict-char-style (markup-stream :terminal-sub)
        (depict-integer markup-stream subscript)))))



;;; ------------------------------------------------------------------------------------------------------
;;; NONTERMINAL PARAMETERS

(declaim (inline nonterminal-parameter?))
(defun nonterminal-parameter? (x)
  (symbolp x))
(deftype nonterminal-parameter () 'symbol)


; Return true if this nonterminal parameter is a constant.
(declaim (inline nonterminal-attribute?))
(defun nonterminal-attribute? (parameter)
  (and (symbolp parameter) (not (keywordp parameter))))
(deftype nonterminal-attribute () '(and symbol (not keyword)))


(defun depict-nonterminal-attribute (markup-stream attribute)
  (depict-char-style (markup-stream :nonterminal-attribute)
    (depict markup-stream (symbol-lower-mixed-case-name attribute))))


; Return true if this nonterminal parameter is a variable.
(declaim (inline nonterminal-argument?))
(defun nonterminal-argument? (parameter)
  (keywordp parameter))
(deftype nonterminal-argument () 'keyword)


(defparameter *special-nonterminal-arguments*
  '(:alpha :beta :gamma :delta :epsilon :zeta :eta :theta :iota :kappa :lambda :mu :nu
    :xi :omicron :pi :rho :sigma :tau :upsilon :phi :chi :psi :omega))

(defun depict-nonterminal-argument (markup-stream argument)
  (depict-char-style (markup-stream :nonterminal-argument)
    (let ((argument (symbol-abbreviation argument)))
      (depict markup-stream
              (if (member argument *special-nonterminal-arguments*)
                argument
                (symbol-upper-mixed-case-name argument))))))


;;; ------------------------------------------------------------------------------------------------------
;;; ATTRIBUTED NONTERMINALS

; An attributed-nonterminal is a specific instantiation of a generic-nonterminal.
(defstruct (attributed-nonterminal (:constructor allocate-attributed-nonterminal (symbol attributes))
                                   (:copier nil)
                                   (:predicate attributed-nonterminal?))
  (symbol nil :type keyword :read-only t)    ;The name of the attributed nonterminal
  (attributes nil :type list :read-only t))  ;Ordered list of nonterminal attributes


; Make an attributed nonterminal with the given symbol and attributes.  If there
; are no attributes, return the symbol as a plain nonterminal.
; Nonterminals are eq whenever they have identical symbols and attribute lists.
(defun make-attributed-nonterminal (symbol attributes)
  (assert-type symbol keyword)
  (assert-type attributes (list nonterminal-attribute))
  (if attributes
    (let ((generic-nonterminals (get symbol 'generic-nonterminals)))
      (or (cdr (assoc attributes generic-nonterminals :test #'equal))
          (let ((attributed-nonterminal (allocate-attributed-nonterminal symbol attributes)))
            (setf (get symbol 'generic-nonterminals)
                  (acons attributes attributed-nonterminal generic-nonterminals))
            attributed-nonterminal)))
    symbol))


(defmethod print-object ((attributed-nonterminal attributed-nonterminal) stream)
  (print-unreadable-object (attributed-nonterminal stream)
    (format stream "a ~@_~W~{ ~:_~W~}"
            (attributed-nonterminal-symbol attributed-nonterminal)
            (attributed-nonterminal-attributes attributed-nonterminal))))


;;; ------------------------------------------------------------------------------------------------------
;;; GENERIC NONTERMINALS

; A generic-nonterminal is a parametrized nonterminal that can expand into two or more
; attributed-nonterminals.
(defstruct (generic-nonterminal (:constructor allocate-generic-nonterminal (symbol parameters))
                                (:copier nil)
                                (:predicate generic-nonterminal?))
  (symbol nil :type keyword :read-only t)    ;The name of the generic nonterminal
  (parameters nil :type list :read-only t))  ;Ordered list of nonterminal attributes or arguments


; Make a generic nonterminal with the given symbol and parameters.  If none of
; the parameters is an argument, make an attributed nonterminal instead.  If there
; are no parameters, return the symbol as a plain nonterminal.
; Nonterminals are eq whenever they have identical symbols and parameter lists.
(defun make-generic-nonterminal (symbol parameters)
  (assert-type symbol keyword)
  (if parameters
    (let ((generic-nonterminals (get symbol 'generic-nonterminals)))
      (or (cdr (assoc parameters generic-nonterminals :test #'equal))
          (progn
            (assert-type parameters (list nonterminal-parameter))
            (let ((generic-nonterminal (if (every #'nonterminal-attribute? parameters)
                                         (allocate-attributed-nonterminal symbol parameters)
                                         (allocate-generic-nonterminal symbol parameters))))
              (setf (get symbol 'generic-nonterminals)
                    (acons parameters generic-nonterminal generic-nonterminals))
              generic-nonterminal))))
    symbol))


(defmethod print-object ((generic-nonterminal generic-nonterminal) stream)
  (print-unreadable-object (generic-nonterminal stream)
    (format stream "g ~@_~W~{ ~:_~W~}"
            (generic-nonterminal-symbol generic-nonterminal)
            (generic-nonterminal-parameters generic-nonterminal))))


;;; ------------------------------------------------------------------------------------------------------
;;; NONTERMINALS

;;; A nonterminal is a keyword or an attributed-nonterminal.
(declaim (inline nonterminal?))
(defun nonterminal? (x)
  (or (keywordp x) (attributed-nonterminal? x)))

; The following nonterminals are reserved and may not be used in user input:
;   :%   Nonterminal that expands to the start nonterminal

(defconstant *start-nonterminal* :%)

(deftype nonterminal () '(or keyword attributed-nonterminal))
(deftype user-nonterminal () `(and nonterminal (not (eql ,*start-nonterminal*))))


;;; ------------------------------------------------------------------------------------------------------
;;; GENERAL NONTERMINALS

;;; A general-nonterminal is a nonterminal or a generic-nonterminal.
(declaim (inline general-nonterminal?))
(defun general-nonterminal? (x)
  (or (nonterminal? x) (generic-nonterminal? x)))

(deftype general-nonterminal () '(or nonterminal generic-nonterminal))


; Return the list of parameters in the general-nonterminal.  The list is empty if the
; general-nonterminal is a plain nonterminal.
(defun general-nonterminal-parameters (general-nonterminal)
  (cond
   ((attributed-nonterminal? general-nonterminal) (attributed-nonterminal-attributes general-nonterminal))
   ((generic-nonterminal? general-nonterminal) (generic-nonterminal-parameters general-nonterminal))
   (t (progn
        (assert-true (keywordp general-nonterminal))
        nil))))


; Emit markup for a general-nonterminal.  subscript is an optional integer.
; link should be one of:
;   :reference   if this is a reference of this general-nonterminal;
;   :external    if this is an external reference of this general-nonterminal;
;   :definition  if this is a definition of this general-nonterminal;
;   nil          if this use of the general-nonterminal should not be cross-referenced.
(defun depict-general-nonterminal (markup-stream general-nonterminal link &optional subscript)
  (labels
    ((depict-nonterminal-name (markup-stream symbol)
       (let ((name (symbol-upper-mixed-case-name symbol)))
         (depict-link (markup-stream link "N-" name t)
           (depict-char-style (markup-stream :nonterminal)
             (depict markup-stream name)))))
     
     (depict-nonterminal-parameter (markup-stream parameter)
       (if (nonterminal-attribute? parameter)
         (depict-nonterminal-attribute markup-stream parameter)
         (depict-nonterminal-argument markup-stream parameter)))
     
     (depict-parametrized-nonterminal (markup-stream symbol parameters)
       (depict-nonterminal-name markup-stream symbol)
       (depict-char-style (markup-stream :superscript)
         (depict-list markup-stream #'depict-nonterminal-parameter parameters
                      :separator ","))))
    
    (cond
     ((keywordp general-nonterminal)
      (depict-nonterminal-name markup-stream general-nonterminal))
     ((attributed-nonterminal? general-nonterminal)
      (depict-parametrized-nonterminal markup-stream
                                       (attributed-nonterminal-symbol general-nonterminal)
                                       (attributed-nonterminal-attributes general-nonterminal)))
     ((generic-nonterminal? general-nonterminal)
      (depict-parametrized-nonterminal markup-stream
                                       (generic-nonterminal-symbol general-nonterminal)
                                       (generic-nonterminal-parameters general-nonterminal)))
     (t (error "Bad nonterminal ~S" general-nonterminal)))
    (when subscript
      (depict-char-style (markup-stream :subscript)
        (depict-char-style (markup-stream :nonterminal-sub)
          (depict-integer markup-stream subscript))))))



;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR SYMBOLS

;;; A grammar-symbol is either a terminal or a nonterminal.
(deftype grammar-symbol () '(or terminal nonterminal))
(deftype user-grammar-symbol () '(or user-terminal user-nonterminal))

;;; A general-grammar-symbol is either a terminal or a general-nonterminal.
(deftype general-grammar-symbol () '(or terminal general-nonterminal))

; Return true if x is a general-grammar-symbol.  x can be any object.
(defun general-grammar-symbol? (x)
  (or (terminal? x) (general-nonterminal? x)))


; Return true if the two grammar symbols are the same symbol.
(declaim (inline grammar-symbol-=))
(defun grammar-symbol-= (grammar-symbol1 grammar-symbol2)
  (eql grammar-symbol1 grammar-symbol2))
; A version of grammar-symbol-= suitable for being the test function for hash tables.
(defparameter *grammar-symbol-=* #'eql)


; Return the general-grammar-symbol's symbol.  Return it unchanged if it is not
; an attributed or generic nonterminal.
(defun general-grammar-symbol-symbol (general-grammar-symbol)
  (cond
   ((attributed-nonterminal? general-grammar-symbol) (attributed-nonterminal-symbol general-grammar-symbol))
   ((generic-nonterminal? general-grammar-symbol) (generic-nonterminal-symbol general-grammar-symbol))
   (t (assert-type general-grammar-symbol (or keyword terminal)))))


; Return the list of arguments in the general-grammar-symbol.  The list is empty if the
; general-grammar-symbol is not a generic nonterminal.
(defun general-grammar-symbol-arguments (general-grammar-symbol)
  (and (generic-nonterminal? general-grammar-symbol)
       (remove-if (complement #'nonterminal-argument?) (generic-nonterminal-parameters general-grammar-symbol))))


; Return the general-grammar-symbol expanded into source form that can be interned to yield the same
; general-grammar-symbol.
(defun general-grammar-symbol-source (general-grammar-symbol)
  (cond
   ((attributed-nonterminal? general-grammar-symbol)
    (cons (attributed-nonterminal-symbol general-grammar-symbol) (attributed-nonterminal-attributes general-grammar-symbol)))
   ((generic-nonterminal? general-grammar-symbol)
    (cons (generic-nonterminal-symbol general-grammar-symbol) (generic-nonterminal-parameters general-grammar-symbol)))
   (t (assert-type general-grammar-symbol (or keyword terminal)))))


; Emit markup for a general-grammar-symbol.  subscript is an optional integer.
; link should be one of:
;   :reference   if this is a reference of this general-grammar-symbol;
;   :external    if this is an external reference of this general-grammar-symbol;
;   :definition  if this is a definition of this general-grammar-symbol;
;   nil          if this use of the general-grammar-symbol should not be cross-referenced.
(defun depict-general-grammar-symbol (markup-stream general-grammar-symbol link &optional subscript)
  (if (general-nonterminal? general-grammar-symbol)
    (depict-general-nonterminal markup-stream general-grammar-symbol link subscript)
    (depict-terminal markup-stream general-grammar-symbol subscript)))


; Styled text can include (:grammar-symbol <grammar-symbol-source> [<subscript>]) as long as
; *styled-text-grammar-parametrization* is bound around the call to depict-styled-text.
(defvar *styled-text-grammar-parametrization*)

(defun depict-grammar-symbol-styled-text (markup-stream grammar-symbol-source &optional subscript)
  (depict-general-grammar-symbol markup-stream
                                 (grammar-parametrization-intern *styled-text-grammar-parametrization* grammar-symbol-source)
                                 :reference
                                 subscript))

(setf (styled-text-depictor :grammar-symbol) #'depict-grammar-symbol-styled-text)


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR PARAMETRIZATIONS

; A grammar parametrization holds the rules for converting nonterminal arguments into nonterminal attributes.
(defstruct (grammar-parametrization (:constructor allocate-grammar-parametrization (argument-attributes))
                                    (:predicate grammar-parametrization?))
  (argument-attributes nil :type hash-table :read-only t))   ;Hash table of nonterminal-argument -> list of nonterminal-attributes


(defun make-grammar-parametrization ()
  (allocate-grammar-parametrization (make-hash-table :test #'eq)))


; Return true if the two grammar-parametrizations are the same.
(defun grammar-parametrization-= (grammar-parametrization1 grammar-parametrization2)
  (hash-table-= (grammar-parametrization-argument-attributes grammar-parametrization1)
                (grammar-parametrization-argument-attributes grammar-parametrization2)
                :test #'equal))


; Declare that nonterminal arguments with the given name can hold any of the
; given nonterminal attributes given.  At least one attribute must be provided.
(defun grammar-parametrization-declare-argument (grammar-parametrization argument attributes)
  (assert-type argument nonterminal-argument)
  (assert-type attributes (list nonterminal-attribute))
  (assert-true attributes)
  (when (gethash argument (grammar-parametrization-argument-attributes grammar-parametrization))
    (error "Duplicate parametrized grammar argument ~S" argument))
  (setf (gethash argument (grammar-parametrization-argument-attributes grammar-parametrization)) attributes))


; Return the attributes to which the given argument may expand.
(defun grammar-parametrization-lookup-argument (grammar-parametrization argument)
  (assert-non-null (gethash argument (grammar-parametrization-argument-attributes grammar-parametrization))))


; Create a plain, attributed, or generic grammar symbol from the specification in grammar-symbol-source.
; If grammar-symbol-source is not a cons, it is a plain grammar symbol.  If it is a list, its first element
; must be a keyword that is a nonterminal's symbol and the other elements must be nonterminal
; parameters.
; Return two values:
;   the grammar symbol
;   a list of arguments used in the grammar symbol.
; If allowed-arguments is given, check that each argument is in the allowed-arguments list;
; if not, allow any arguments declared in grammar-parametrization but do not allow duplicates.
(defun grammar-parametrization-intern (grammar-parametrization grammar-symbol-source &optional (allowed-arguments nil allow-duplicates))
  (if (consp grammar-symbol-source)
    (progn
      (assert-type grammar-symbol-source (cons keyword (list nonterminal-parameter)))
      (let* ((symbol (car grammar-symbol-source))
             (parameters (cdr grammar-symbol-source))
             (arguments (remove-if (complement #'nonterminal-argument?) parameters)))
        (mapl #'(lambda (arguments)
                  (let ((argument (car arguments)))
                    (if allow-duplicates
                      (unless (member argument allowed-arguments :test #'eq)
                        (error "Undefined nonterminal argument ~S" argument))
                      (progn
                        (unless (gethash argument (grammar-parametrization-argument-attributes grammar-parametrization))
                          (error "Undeclared nonterminal argument ~S" argument))
                        (when (member argument (cdr arguments) :test #'eq)
                          (error "Duplicate nonterminal argument ~S" argument))))))
              arguments)
        (values (make-generic-nonterminal symbol parameters) arguments)))
    (values (assert-type grammar-symbol-source (or keyword terminal)) nil)))


; Call f on each possible binding permutation of the given arguments concatenated with the bindings in
; bound-argument-alist.  f takes one argument, an association list that maps arguments to attributes.
(defun grammar-parametrization-each-permutation (grammar-parametrization f arguments &optional bound-argument-alist)
  (if arguments
    (let ((argument (car arguments))
          (rest-arguments (cdr arguments)))
      (dolist (attribute (grammar-parametrization-lookup-argument grammar-parametrization argument))
        (grammar-parametrization-each-permutation grammar-parametrization f rest-arguments (acons argument attribute bound-argument-alist))))
    (funcall f bound-argument-alist)))


; If general-grammar-symbol is a generic-nonterminal, return one possible binding permutation of its arguments;
; otherwise return nil.
(defun nonterminal-sample-bound-argument-alist (grammar-parametrization general-grammar-symbol)
  (when (generic-nonterminal? general-grammar-symbol)
    (grammar-parametrization-each-permutation
     grammar-parametrization
     #'(lambda (bound-argument-alist) (return-from nonterminal-sample-bound-argument-alist bound-argument-alist))
     (general-grammar-symbol-arguments general-grammar-symbol))))


; If the grammar symbol is a generic nonterminal, convert it into an attributed nonterminal
; by instantiating its arguments with the corresponding attributes from the bound-argument-alist.
; If the grammar symbol is already an attributed or plain nonterminal, return it unchanged.
(defun instantiate-general-grammar-symbol (bound-argument-alist general-grammar-symbol)
  (if (generic-nonterminal? general-grammar-symbol)
    (make-attributed-nonterminal
     (generic-nonterminal-symbol general-grammar-symbol)
     (mapcar #'(lambda (parameter)
                 (if (nonterminal-argument? parameter)
                   (let ((binding (assoc parameter bound-argument-alist :test #'eq)))
                     (if binding
                       (cdr binding)
                       (error "Unbound nonterminal argument ~S" parameter)))
                   parameter))
             (generic-nonterminal-parameters general-grammar-symbol)))
    (assert-type general-grammar-symbol grammar-symbol)))


; If the grammar symbol is a generic nonterminal parametrized on argument, substitute
; attribute for argument in it and return the modified grammar symbol.  Otherwise, return it unchanged.
(defun general-grammar-symbol-substitute (attribute argument general-grammar-symbol)
  (assert-type attribute nonterminal-attribute)
  (assert-type argument nonterminal-argument)
  (if (and (generic-nonterminal? general-grammar-symbol)
           (member argument (generic-nonterminal-parameters general-grammar-symbol) :test #'eq))
    (make-generic-nonterminal
     (generic-nonterminal-symbol general-grammar-symbol)
     (substitute attribute argument (generic-nonterminal-parameters general-grammar-symbol) :test #'eq))
    (assert-type general-grammar-symbol general-grammar-symbol)))


; If the general grammar symbol is a generic nonterminal, return a list of all possible attributed nonterminals
; that can be instantiated from it; otherwise, return a one-element list containing the given general grammar symbol.
(defun general-grammar-symbol-instances (grammar-parametrization general-grammar-symbol)
  (if (generic-nonterminal? general-grammar-symbol)
    (let ((instances nil))
      (grammar-parametrization-each-permutation
       grammar-parametrization
       #'(lambda (bound-argument-alist)
           (push (instantiate-general-grammar-symbol bound-argument-alist general-grammar-symbol) instances))
       (general-grammar-symbol-arguments general-grammar-symbol))
      (nreverse instances))
    (list (assert-type general-grammar-symbol grammar-symbol))))


; Return true if grammar-symbol can be obtained by calling instantiate-general-grammar-symbol on
; general-grammar-symbol.
(defun general-nonterminal-is-instance? (grammar-parametrization general-grammar-symbol grammar-symbol)
  (or (grammar-symbol-= general-grammar-symbol grammar-symbol)
      (and (generic-nonterminal? general-grammar-symbol)
           (attributed-nonterminal? grammar-symbol)
           (let ((parameters (generic-nonterminal-parameters general-grammar-symbol))
                 (attributes (attributed-nonterminal-attributes grammar-symbol)))
             (and (= (length parameters) (length attributes))
                  (every #'(lambda (parameter attribute)
                             (or (eq parameter attribute)
                                 (and (nonterminal-argument? parameter)
                                      (member attribute (grammar-parametrization-lookup-argument grammar-parametrization parameter) :test #'eq))))
                         parameters
                         attributes))))))
