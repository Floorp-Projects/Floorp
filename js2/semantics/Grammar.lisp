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
;;; LALR(1) and LR(1) grammar generator
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


;;; ------------------------------------------------------------------------------------------------------
;;; TERMINALSETS

;;; A set of terminals is a bitset represented by an integer, indexed by terminal numbers.

(deftype terminalset () 'integer)

(defconstant *empty-terminalset* 0)
(defconstant *full-terminalset* -1)

; Return true if terminalset is empty.
(declaim (inline terminalset-empty?))
(defun terminalset-empty? (terminalset)
  (zerop terminalset))


; Return true if the two terminalsets are equal.
(declaim (inline terminalset-=))
(defun terminalset-= (terminalset1 terminalset2)
  (= terminalset1 terminalset2))


; Return true if the terminalset1 is a subset of (or equal to) terminalset2.
(declaim (inline terminalset-<=))
(defun terminalset-<= (terminalset1 terminalset2)
  (zerop (logandc2 terminalset1 terminalset2)))


; Return true if the two terminalsets are disjoint.
(declaim (inline terminalsets-disjoint))
(defun terminalsets-disjoint (terminalset1 terminalset2)
  (zerop (logand terminalset1 terminalset2)))


; Return the complement of the terminalset.
(declaim (inline terminalset-complement))
(defun terminalset-complement (terminalset)
  (lognot terminalset))


; Return the intersection of the two terminalsets.
(declaim (inline terminalset-intersection))
(defun terminalset-intersection (terminalset1 terminalset2)
  (logand terminalset1 terminalset2))

(define-modify-macro terminalset-intersection-f (terminalset2) terminalset-intersection)


; Return the union of the two terminalsets.
(declaim (inline terminalset-union))
(defun terminalset-union (terminalset1 terminalset2)
  (logior terminalset1 terminalset2))

(define-modify-macro terminalset-union-f (terminalset2) terminalset-union)


; Return the elements in terminalset1 that are not in terminalset2.
(declaim (inline terminalset-difference))
(defun terminalset-difference (terminalset1 terminalset2)
  (logandc2 terminalset1 terminalset2))

(define-modify-macro terminalset-difference-f (terminalset2) terminalset-difference)


; Return a unique serial number for the given terminal.
(declaim (inline terminal-number))
(defun terminal-number (grammar terminal)
  (assert-non-null (gethash terminal (grammar-terminal-numbers grammar))
                   "Can't find terminal ~S" terminal))


; Return true if the terminal is present in the terminalset.
(defun terminal-in-terminalset (grammar terminal terminalset)
  (logbitp (terminal-number grammar terminal) terminalset))


; Return a one-element terminalset containing the given terminal.
(declaim (inline make-terminalset))
(defun make-terminalset (grammar terminal)
  (ash 1 (terminal-number grammar terminal)))


; Return a terminalset containing all terminals in the grammar.
(defun make-full-terminalset (grammar)
  (1- (ash 1 (length (grammar-terminals grammar)))))


; Call f on every terminal in the terminalset in reverse order.
(defun map-terminalset-reverse (f grammar terminalset)
  (assert-true (>= terminalset 0))
  (do ()
      ((zerop terminalset))
    (let ((last (1- (integer-length terminalset))))
      (funcall f (svref (grammar-terminals grammar) last))
      (setq terminalset (- terminalset (ash 1 last))))))


; Return a list containing the terminals in the terminalset.
(defun terminalset-list (grammar terminalset)
  (let ((terminals nil))
    (map-terminalset-reverse #'(lambda (terminal) (push terminal terminals))
                             grammar terminalset)
    terminals))


(defun print-terminalset (grammar terminalset &optional (stream t))
  (let ((first t))
    (dolist (terminal (terminalset-list grammar terminalset))
      (if first
        (setq first nil)
        (format stream " ~:_"))
      (write terminal :stream stream))))


;;; ------------------------------------------------------------------------------------------------------
;;; ACTIONS

;;; An action describes one semantic function associated with a production.
;;; The action code is a lisp form that evaluates to a function f that takes
;;; the following arguments:
;;;   the values returned by the rhs's first grammar symbol's actions, in order of the
;;;     actions of that grammar symbol;
;;;   the values returned by the rhs's second grammar symbol's actions, in order of the
;;;     actions of that grammar symbol;
;;;   ...
;;;   the values returned by the rhs's last grammar symbol's actions, in order of the
;;;     actions of that grammar symbol.
;;; Function f returns one value, which is the result of this action.
(defstruct (action (:constructor make-action (type expr))
                   (:predicate action?))
  (type nil :read-only t)                     ;The unparsed type of the action's result
  (expr nil :read-only t)                     ;The unparsed source expression that defines the action
  (code nil))                                 ;The generated lisp source code that performs the action


;;; ------------------------------------------------------------------------------------------------------
;;; CONSTRAINTS

;;; A constraint modifies the rhs of a production.  The constraint applies just before the pos-th
;;; general grammar symbol on the rhs of the production.
(defstruct (constraint (:constructor nil) (:copier nil) (:predicate constraint?))
  (pos nil :type integer :read-only t)             ;Position of this constraint; ranges between 0 and length(general-production-rhs), inclusive.
  (terminalset nil :type (or null terminalset)))   ;Set of allowed lookahead terminals; null until terminals are numbered


;;; A lookahead-constraint imposes a restriction on the current lookahead terminal when matching
;;; against the production.  The match succeeds only if there is no following terminal or the following
;;; terminal is not present in the lookahead-constraint's list of forbidden terminals.
(defstruct (lookahead-constraint (:include constraint)
                                 (:constructor make-lookahead-constraint (pos forbidden-terminals source))
                                 (:predicate lookahead-constraint?))
  (forbidden-terminals nil :type list :read-only t) ;List of forbidden terminals
  (source nil :type list :read-only t))             ;List of grammar symbols (terminals or nonterminals) that produced the list of forbidden terminals


; Emit markup for a lookahead-constraint.
(defun depict-lookahead-constraint (markup-stream lookahead-constraint)
  (depict markup-stream :begin-negative-lookahead)
  (depict-list markup-stream
               #'depict-production-rhs-component
               (lookahead-constraint-source lookahead-constraint)
               :separator ", ")
  (depict markup-stream :end-negative-lookahead))


(defmethod print-object ((lookahead-constraint lookahead-constraint) stream)
  (print-unreadable-object (lookahead-constraint stream)
    (format stream "-~{ ~:_~W~}" (lookahead-constraint-source lookahead-constraint))))


;;; A variant-constraint succeeds only when there is no line break at the current position.
(defstruct (variant-constraint (:include constraint)
                               (:constructor make-variant-constraint (pos name))
                               (:predicate variant-constraint?))
  (name nil :read-only t))                         ;This variant-constraint's depict name


; Emit markup for a variant-constraint.
(defun depict-variant-constraint (markup-stream variant-constraint)
  (depict markup-stream (variant-constraint-name variant-constraint)))


(defmethod print-object ((variant-constraint variant-constraint) stream)
  (print-unreadable-object (variant-constraint stream)
    (write (variant-constraint-name variant-constraint) :stream stream)))


;;; ------------------------------------------------------------------------------------------------------
;;; GENERALIZED PRODUCTIONS

;;; A general-production is a common base class for the production and generic-production classes.
(defstruct (general-production (:constructor nil) (:copier nil) (:predicate general-production?))
  (lhs nil :type general-nonterminal :read-only t) ;The general-nonterminal on the left-hand side of this general-production
  (rhs nil :type list :read-only t)                ;List of general grammar symbols to which that general-nonterminal expands
  (constraints nil :type list :read-only t)        ;List of constraints applying to rhs, sorted by increasing pos values
  (name nil :read-only t)                          ;This general-production's name that will be used to name the parse tree node
  (highlight nil :read-only t))                    ;This general-production's markup style keyword or nil if default


; If general-production is a generic-production, return its list of productions;
; if general-production is a production, return it in a one-element list.
(defun general-production-productions (general-production)
  (if (production? general-production)
    (list general-production)
    (generic-production-productions general-production)))


; Return the general-production's rhs with constraints interleaved with the rhs general grammar symbols.
(defun general-production-rhs-components (general-production)
  (labels
    ((merge-constraints (rhs constraints pos)
       (if constraints
         (let* ((constraint (first constraints))
                (constraint-pos (constraint-pos constraint)))
           (cond
            ((= constraint-pos pos)
             (cons constraint (merge-constraints rhs (rest constraints) pos)))
            (rhs
             (cons (first rhs) (merge-constraints (rest rhs) constraints (1+ pos))))
            (t (error "Bad constraint list"))))
         rhs)))
    
    (merge-constraints (general-production-rhs general-production)
                       (general-production-constraints general-production)
                       0)))


; Return the general-production's constraint's terminalset at the given position.
(defun general-production-constraint (general-production pos)
  (let ((terminalset *full-terminalset*))
    (dolist (constraint (general-production-constraints general-production))
      (when (= (constraint-pos constraint) pos)
        (terminalset-intersection-f terminalset (constraint-terminalset constraint))))
    terminalset))


; Emit a markup paragraph for the left-hand-side of a general production.
(defun depict-general-production-lhs (markup-stream lhs-general-nonterminal)
  (depict-paragraph (markup-stream :grammar-lhs)
    (depict-general-nonterminal markup-stream lhs-general-nonterminal :definition)
    (depict markup-stream " " :derives-10)))


; Emit markup for a production right-hand-side component.
(defun depict-production-rhs-component (markup-stream production-rhs-component &optional subscript)
  (cond
   ((lookahead-constraint? production-rhs-component)
    (depict-lookahead-constraint markup-stream production-rhs-component))
   ((variant-constraint? production-rhs-component)
    (depict-variant-constraint markup-stream production-rhs-component))
   (t (depict-general-grammar-symbol markup-stream production-rhs-component :reference subscript))))


; Emit a markup paragraph for the right-hand-side of a general production.
; first is true if this is the first production in a rule.
; last is true if this is the last production in a rule.
(defun depict-general-production-rhs (markup-stream general-production first last)
  (depict-paragraph (markup-stream (if last :grammar-rhs-last :grammar-rhs))
    (if first
      (depict markup-stream :tab3)
      (depict markup-stream "|" :tab2))
    (let ((rhs-components (general-production-rhs-components general-production)))
      (depict-list markup-stream
                   #'depict-production-rhs-component
                   rhs-components
                   :separator " "
                   :empty '(:left-angle-quote "empty" :right-angle-quote)))))


; Emit the general production, including both its left and right-hand sides.
; Include serial number subscripts on all rhs grammar symbols that both
;    appear more than once in the rhs or appear in the lhs, and
;    have symbols that are present in the symbols-with-subscripts list.
; link is the lhs's link type.
(defun depict-general-production (markup-stream general-production link &optional symbols-with-subscripts)
  (let ((lhs (general-production-lhs general-production))
        (rhs-components (general-production-rhs-components general-production)))
    (depict-general-nonterminal markup-stream lhs link)
    (depict markup-stream " " :derives-10)
    (if rhs-components
      (let ((counts-hash (make-hash-table :test *grammar-symbol-=*)))
        (when symbols-with-subscripts
          (dolist (symbol symbols-with-subscripts)
            (setf (gethash symbol counts-hash) 0))
          (dolist (production-rhs-component (cons lhs (general-production-rhs general-production)))
            (when (general-grammar-symbol? production-rhs-component)
              (let ((symbol (general-grammar-symbol-symbol production-rhs-component)))
                (when (gethash symbol counts-hash)
                  (incf (gethash symbol counts-hash))))))
          (maphash #'(lambda (symbol count)
                       (assert-true (> count 0))
                       (if (> count 1)
                         (setf (gethash symbol counts-hash) 0)
                         (remhash symbol counts-hash)))
                   counts-hash))
        (dolist (production-rhs-component rhs-components)
          (let ((subscript nil))
            (when (general-grammar-symbol? production-rhs-component)
              (let ((symbol (general-grammar-symbol-symbol production-rhs-component)))
                (when (gethash symbol counts-hash)
                  (setq subscript (incf (gethash symbol counts-hash))))))
            (depict-space markup-stream)
            (depict-production-rhs-component markup-stream production-rhs-component subscript))))
      (depict markup-stream " " :left-angle-quote "empty" :right-angle-quote))))


;;; ------------------------------------------------------------------------------------------------------
;;; PRODUCTIONS

;;; A production describes the expansion of a nonterminal (the lhs) into
;;; a string of zero or more grammar symbols (the rhs); the rhs may also have constraints attached.
;;; Each production has a unique number.  Earlier productions have smaller numbers.
;;; There is exactly one production structure for a given production, so eq can be
;;; used to test for production equality.
;;;
;;; The evaluator is a lisp form that evaluates to a function f that takes one argument --
;;; the old state of the parser's value stack -- and returns the new state of that stack.
(defstruct (production (:include general-production (lhs nil :type nonterminal :read-only t))
                       (:constructor make-production (lhs rhs constraints name highlight rhs-length number))
                       (:copier nil) (:predicate production?))
  (rhs-length nil :type integer :read-only t) ;Number of grammar symbols in the rhs
  (number nil :type integer :read-only t)     ;This production's serial number
  ;The following fields are used for the action generator.
  (actions nil :type list)                    ;List of (action-symbol . action-or-nil) in the same order as the action-symbols
  ;                                           ; are listed in the grammar's action-signatures hash table for this lhs
  (n-action-args nil :type (or null integer)) ;Total size of the action-signatures of all grammar symbols in the rhs
  (evaluator nil :type (or null function)))   ;The lisp evaluator of the action


; Return a list of terminals in this production's rhs.
; The list may contain duplicates.
(declaim (inline production-terminals))
(defun production-terminals (production)
  (remove-if (complement #'terminal?) (production-rhs production)))


; Return a list of nonterminals in this production's rhs.
; The list may contain duplicates.
(declaim (inline production-nonterminals))
(defun production-nonterminals (production)
  (remove-if (complement #'nonterminal?) (production-rhs production)))


(defun print-production (production &optional (stream t))
  (format stream "~<~W -> ~:I~_~:[<epsilon> ~;~:*~{~W ~:_~}~]~:> ~:_[~W]"
          (list (production-lhs production) (general-production-rhs-components production))
          (production-name production)))


;;; ------------------------------------------------------------------------------------------------------
;;; GENERIC PRODUCTIONS

;;; A generic production describes a set of productions generated by instantiating a
;;; production with a generic nonterminal on the lhs.  All productions in this set are
;;; generated by replacing the arguments in the lhs nonterminal with attributes and making
;;; corresponding replacements on the production's rhs.  All productions in this set share
;;; the same name.  Note that this set might not cover all productions with the same
;;; nonterminal symbol on the lhs because some parameters of the lhs nonterminal might be
;;; originally listed as attributes and not arguments.
;;; A generic production is not a production and does not have a number or actions.

(defstruct (generic-production (:include general-production (lhs nil :type generic-nonterminal :read-only t))
                               (:constructor make-generic-production (lhs rhs constraints name highlight productions))
                               (:copier nil)
                               (:predicate generic-production?))
  (productions nil :type list :read-only t))       ;List of instantiations of this generic production


; Return a new generic-production or production with the given nonterminal argument replaced by the given attribute.
; If the resulting production isn't generic, an existing attributed production is returned.
(defun generic-production-substitute (grammar-parametrization attribute argument generic-production)
  (let ((new-lhs (general-grammar-symbol-substitute attribute argument (generic-production-lhs generic-production)))
        (productions (generic-production-productions generic-production)))
    (if (generic-nonterminal? new-lhs)
      (make-generic-production
       new-lhs
       (mapcar #'(lambda (grammar-symbol)
                   (general-grammar-symbol-substitute attribute argument grammar-symbol))
               (generic-production-rhs generic-production))
       (general-production-constraints generic-production)
       (generic-production-name generic-production)
       (generic-production-highlight generic-production)
       (remove-if #'(lambda (production)
                      (not (general-nonterminal-is-instance? grammar-parametrization new-lhs (production-lhs production))))
                  productions))
      
      (assert-non-null (find new-lhs productions :key #'production-lhs :test #'eq)))))



;;; ------------------------------------------------------------------------------------------------------
;;; GENERALIZED RULES

;;; A general-rule is a common base class for the rule and generic-rule classes.
(defstruct (general-rule (:constructor nil) (:copier nil) (:predicate general-rule?))
  (productions nil :type list))              ;The list of all general productions for this rule's general nonterminal lhs


; Return this rule's lhs (which is the same for all productions).
(declaim (inline general-rule-lhs))
(defun general-rule-lhs (general-rule)
  (general-production-lhs (first (general-rule-productions general-rule))))


; Return the given highlight.  If it is nil, return nil and ensure that no other
; highlight (from the list given by highlights) is currently in effect in the markup-stream.
(defun check-highlight (highlight highlights markup-stream)
  (if highlight
    (assert-true (member highlight highlights))
    (dolist (h highlights)
      (ensure-no-enclosing-style markup-stream h)))
  highlight)


; Return the list of general-productions, in order, gathered into runs of consecutive productions with the same highlight value.
; The result is a list of runs:
;   (<highlight> <p> ... <p>),
; where each <p> is:
;   (<general-production> <first> <last>),
; where <first> is true if this is the first production and <last> is true if this is the last production.
(defun gather-productions-by-highlights (general-productions)
  (when general-productions
    (let* ((first-production (first general-productions))
           (prior-runs-reverse nil)
           (current-highlight (general-production-highlight first-production))
           (current-run-productions-reverse (list (list first-production t nil))))
      (dolist (general-production (rest general-productions))
        (let ((highlight (general-production-highlight general-production))
              (p (list general-production nil nil)))
          (if (eq highlight current-highlight)
            (push p current-run-productions-reverse)
            (progn
              (push (cons current-highlight (nreverse current-run-productions-reverse)) prior-runs-reverse)
              (setq current-highlight highlight)
              (setq current-run-productions-reverse (list p))))))
      (setf (third (first current-run-productions-reverse)) t)
      (nreconc prior-runs-reverse (list (cons current-highlight (nreverse current-run-productions-reverse)))))))


; Emit markup paragraphs for the grammar general rule.
; If the rule is short enough (only one production), emit the rule on one line.
; highlights is a list of keywords that may be used to highlight specific rules or productions.  It should
; be the same as the list of highlights passed to make-grammar.
(defun depict-general-rule (markup-stream general-rule highlights)
  (let ((general-productions (general-rule-productions general-rule)))
    (assert-true general-productions)
    (let* ((production-runs (gather-productions-by-highlights general-productions))
           (rule-highlight (and (endp (rest production-runs))
                                (check-highlight (first (first production-runs)) highlights markup-stream))))
      (depict-division-style (markup-stream rule-highlight t)
        (depict-division-style (markup-stream :nowrap)
          (depict-division-style (markup-stream :grammar-rule)
            (if (rest general-productions)
              (progn
                (depict-general-production-lhs markup-stream (general-rule-lhs general-rule))
                (dolist (production-run production-runs)
                  (depict-division-style (markup-stream (check-highlight (first production-run) highlights markup-stream) t)
                    (dolist (p (rest production-run))
                      (apply #'depict-general-production-rhs markup-stream p)))))
              (depict-paragraph (markup-stream :grammar-lhs-last)
                (depict-general-production markup-stream (first general-productions) :definition)))))))))


;;; ------------------------------------------------------------------------------------------------------
;;; RULES

;;; A rule is the set of all productions with the same lhs nonterminal.
;;; There is exactly one rule structure for a given nonterminal lhs, so eq can be
;;; used to test for rule equality.
;;;
;;; If the rule cannot somehow produce an empty expansion, then passthrough-terminals is empty.
;;; Otherwise, passthrough-terminals summarizes the constraints imposed on the next lookahead terminal
;;; imposed by all empty expansions of this rule.  If these empty expansions do not impose any
;;; lookahead-constraints, then passthrough-terminals will be the full set.
(defstruct (rule (:include general-rule (productions nil :type list :read-only t))
                 (:constructor make-rule (productions))
             (:copier nil)
             (:predicate rule?))
  ;productions                                ;The list of all productions for this rule's nonterminal lhs
  (number nil :type (or null integer))        ;This nonterminal's serial number
  (passthrough-terminals *empty-terminalset* :type terminalset) ;See above
  (initial-terminals *empty-terminalset* :type terminalset))    ;Set of all terminals that can begin some expansion of this nonterminal


; Return a list of nonterminals in this rule's rhs.
; The list may contain duplicates.
(defun rule-nonterminals (rule)
  (mapcan #'(lambda (production) (copy-list (production-nonterminals production)))
          (rule-productions rule)))


; Return a unique serial number for the given nonterminal.
(defun nonterminal-number (grammar nonterminal)
  (rule-number (assert-non-null (gethash nonterminal (grammar-rules grammar))
                                "Can't find nonterminal ~S" nonterminal)))


;;; ------------------------------------------------------------------------------------------------------
;;; GENERIC RULES

;;; A generic rule is the set of all generic productions with the same lhs generic nonterminal.
(defstruct (generic-rule (:include general-rule)
                         (:constructor make-generic-rule (productions))
                         (:copier nil)
                         (:predicate generic-rule?)))
;  productions                                ;The list of all generic-productions for this rule's generic nonterminal lhs


; Return a new generic-rule with the given nonterminal argument replaced by the given attribute.
; The resulting rule must still be generic -- it is an error to call this so that it would result
; in a rule with a plain or attributed lhs nonterminal.
(defun generic-rule-substitute (grammar-parametrization attribute argument generic-rule)
  (make-generic-rule
   (mapcar #'(lambda (generic-production)
               (assert-type (generic-production-substitute grammar-parametrization attribute argument generic-production)
                            generic-production))
           (generic-rule-productions generic-rule))))


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR ITEMS

;;; An item is a tuple <prod, position>, where prod is a production and
;;; position is an integer between 0 and length(rhs(production)), inclusive.
;;; The first position elements of production's rhs are called "seen"
;;; and the rest are called "unseen".
;;; There is exactly one item structure for a given <prod, position> combination,
;;; so eq can be used to test for item equality.
(defstruct (item (:constructor allocate-item (production dot unseen number next))
                 (:copier nil)
                 (:predicate item?))
  (production nil :type production :read-only t) ;The production to which this item corresponds
  (dot nil :type integer :read-only t)           ;List of grammar symbols to which that nonterminal expands
  (unseen nil :type list :read-only t)           ;Portion of production's rhs to the right of the dot
  (number nil :type integer :read-only t)        ;Unique number (i.e. different from any other item's number)
  (next nil :type item :read-only t))            ;The item with the same production but dot shifted one to the right; nil if unseen is nil


; Return the first grammar symbol to the right of the item's dot or nil if
; the dot is already at the rightmost position.
(declaim (inline item-next-symbol))
(defun item-next-symbol (item)
  (first (item-unseen item)))


; Return the lhs of the item's production.
(declaim (inline item-lhs))
(defun item-lhs (item)
  (production-lhs (item-production item)))


; Return the constraints of the item's production.
(declaim (inline item-constraints))
(defun item-constraints (item)
  (production-constraints (item-production item)))


; Make an item with the given production and dot location (which must be an integer
; between 0 and length(rhs(production)), inclusive.  Reuse an existing item in the
; grammar if possible.
(defun make-item (grammar production dot)
  (let* ((items-hash (grammar-items-hash grammar))
         (key (cons production dot))
         (existing-item (gethash key items-hash)))
    (or
     existing-item
     (progn
       (assert-true (<= dot (length (production-rhs production))))
       (let* ((unseen (nthcdr dot (production-rhs production)))
              (next (and unseen (make-item grammar production (1+ dot))))
              (number (+ (* (production-number production) (1+ (grammar-max-production-length grammar))) dot)))
         (setf (gethash key items-hash)
               (allocate-item production dot unseen number next)))))))


(defun print-item (item &optional (stream t) after-dot)
  (let ((production (item-production item)))
    (format stream "~W -> ~:_" (production-lhs production))
    (pprint-logical-block (stream (production-rhs production))
      (do ((pos 0 (1+ pos))
           (constraints (production-constraints production))
           (first t))
          (nil)
        (when (= pos (item-dot item))
          (if first
            (setq first nil)
            (format stream " ~:_"))
          (write-char #\. stream)
          (when after-dot
            (format stream " ~:_~W" after-dot)))
        (do ()
            ((or (endp constraints) (/= (constraint-pos (first constraints)) pos)))
          (if first
            (setq first nil)
            (format stream " ~:_"))
          (write (pop constraints) :stream stream))
        (pprint-exit-if-list-exhausted)
        (if first
          (setq first nil)
          (format stream " ~:_"))
        (write (pprint-pop) :stream stream)))))

(defmethod print-object ((item item) stream)
  (print-unreadable-object (item stream)
    (format stream "item ~@_")
    (print-item item stream)))


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR LAITEMS

;;; A laitem is an item with associated lookahead information.
;;; Unlike items, laitem structures are not shared among the states.
(defstruct (laitem (:constructor allocate-laitem (grammar item forbidden lookaheads))
                   (:copier nil)
                   (:predicate laitem?))
  (grammar nil :type grammar :read-only t) ;The grammar to which this laitem belongs
  (item nil :type item :read-only t)       ;The item to which this laitem corresponds
  (forbidden nil :type terminalset)        ;A set of terminals that must not occur after the dot because of lookahead-constraints
  (lookaheads nil :type terminalset)       ;Set of lookahead terminals
  (propagates nil :type list))             ;List of (laitem . mask) to which lookaheads propagate from this laitem (see note below)
;When parsing a LALR(1) grammar, propagates contains all laitems (in this and other states)
;to which lookaheads propagate from this laitem.  The mask in each entry of propagates
;is a terminalset that indicates which lookaheads can propagate; this is usually *full-terminalset*
;but can be smaller in the presence of constraints.
;When parsing a LR(1) grammar, propagates is the list of all laitems to which lookaheads propagate from this laitem
;without following a shift transition.  Such laitems must necessarily be in the same state.  In the LR(1) case each
;laitem listed in the propagates list must come after this laitem in this state's laitems list.


; Add or modify a propagation entry in src-laitem to point to dst-laitem with
; the given mask.
(defun laitem-add-propagation (src-laitem dst-laitem mask)
  (let ((propagation-entry (assoc dst-laitem (laitem-propagates src-laitem))))
    (if propagation-entry
      (terminalset-union-f (cdr propagation-entry) mask)
      (push (cons dst-laitem mask) (laitem-propagates src-laitem)))))


(defvar *lookahead-print-column* 70)

(defun print-laitem (laitem &optional (stream t))
  (let* ((grammar (laitem-grammar laitem))
         (item (laitem-item laitem))
         (forbidden (laitem-forbidden laitem))
         (forbidden-as-constraint
          (and (not (terminalset-empty? forbidden))
               (let ((forbidden-terminals (terminalset-list grammar forbidden)))
                 (make-lookahead-constraint (item-dot item) forbidden-terminals forbidden-terminals)))))
    (print-item item stream forbidden-as-constraint)
    (format stream " ~vT~_" *lookahead-print-column*)
    (pprint-logical-block (stream nil :prefix "{" :suffix "}")
      (print-terminalset grammar (laitem-lookaheads laitem) stream))))

(defmethod print-object ((laitem laitem) stream)
  (print-unreadable-object (laitem stream)
    (format stream "laitem ~@_")
    (print-laitem laitem stream)))


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR TRANSITIONS

;;; A grammar transition is one of the following:
;;;   (:shift state)        ;Shift and go to the given state
;;;   (:reduce production)  ;Reduce by the given production
;;;   (:accept)             ;Accept the input

(declaim (inline make-shift-transition))
(defun make-shift-transition (state)
  (assert-type state state)
  (list :shift state))

(declaim (inline make-reduce-transition))
(defun make-reduce-transition (production)
  (assert-type production production)
  (list :reduce production))

(declaim (inline make-accept-transition))
(defun make-accept-transition ()
  (list :accept))


(declaim (inline transition-kind))
(defun transition-kind (transition)
  (first transition))

(declaim (inline transition-state))
(defun transition-state (transition)
  (assert-true (eq (first transition) :shift))
  (second transition))

(defun (setf transition-state) (state transition)
  (assert-true (eq (first transition) :shift))
  (setf (second transition) state))

(declaim (inline transition-production))
(defun transition-production (transition)
  (assert-true (eq (first transition) :reduce))
  (second transition))


(defun print-transition (transition stream)
  (case (transition-kind transition)
    (:shift (format stream "shift S~D" (state-number (transition-state transition))))
    (:reduce (format stream "reduce ~W" (production-name (transition-production transition))))
    (:accept (format stream "accept"))
    (t (error "Bad transition: ~S" transition))))


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR STATES

(defstruct (state (:constructor allocate-state (number kernel laitems))
                  (:copier nil)
                  (:predicate state?))
  (number nil :type integer :read-only t)  ;Serial number of the state
  (kernel nil :type list)                  ;List of kernel items [list of (item . lookahead) for canonical LR(1)] in order of increasing item-number values
  (laitems nil :type list)                 ;List of laitems
  (transitions nil :type list)             ;List of (terminal . transition).  A null terminal indicates "all terminals".
  (gotos nil :type list))                  ;List of (nonterminal . state)


; Return the transition for the given terminal or nil if there is none.
(defun state-transition (state terminal)
  (let ((transitions (state-transitions state)))
    (cdr 
     (or (assoc terminal transitions :test *grammar-symbol-=*)
         (assoc nil transitions :test *grammar-symbol-=*)))))


; If the state has the same transition for every terminal, return that transition;
; otherwise return nil.
(defun state-only-transition (state)
  (let ((transitions (state-transitions state)))
    (and transitions
         (null (cdr transitions))
         (null (caar transitions))
         (cdar transitions))))


; If all outgoing transitions from the state are the same reduction, return that
; reduction's production; otherwise return nil.
(defun forwarding-state-production (state)
  (let ((transitions (state-transitions state)))
    (and transitions
         (let ((transition (cdar transitions)))
           (and (eq (transition-kind transition) :reduce)
                (null (rassoc transition (cdr transitions) :test (complement #'equal)))
                (transition-production transition))))))


; Return the laitem corresponding to the given item in the given state.
(declaim (inline state-laitem))
(defun state-laitem (state item)
  (assert-non-null (find item (state-laitems state) :key #'laitem-item)
                   "State ~S doesn't have item ~S" state item))


; Return true if this laitem is in the state's kernel either because the dot
; is not at the leftmost position or because the lhs is the start nonterminal.
(defun laitem-in-kernel? (laitem)
  (let ((item (laitem-item laitem)))
    (or (not (zerop (item-dot item)))
        (grammar-symbol-= (item-lhs item) *start-nonterminal*))))


; Return the state's list of laitems sorted by the criteria below.
; Criterion 1 is the major sorting order, criterion 2 decides the sorting order of items
; equal by criterion 1, etc.
;   1.  laitems whose lhs matches the lhs of any kernel item in this state come before
;       laitems whose lhs does not match the lhs of any kernel item in this state.
;   2.  laitems are sorted by the number of their lhs nonterminal.
;   3.  laitems in this state's kernel come before laitems not in the kernel.
;   4.  laitems earlier in (state-laitems state) come before laitems later in (state-laitems state).
(defun laitems-sorted-nicely (state)
  (let ((laitems (copy-list (state-laitems state))))
    (when laitems
      (let ((grammar (laitem-grammar (first laitems))))
        (labels
          ((lhs-matches-some-kernel-item (lhs-nonterminal)
             (member lhs-nonterminal (state-kernel state)
                     :test *grammar-symbol-=*
                     :key #'(lambda (kernel-item)
                              (when (consp kernel-item)
                                (setq kernel-item (car kernel-item)))
                              (item-lhs kernel-item))))
           (laitem-< (laitem1 laitem2)
             (let* ((item1 (laitem-item laitem1))
                    (item2 (laitem-item laitem2))
                    (lhs1 (item-lhs item1))
                    (lhs2 (item-lhs item2))
                    (lhs-number-1 (nonterminal-number grammar lhs1))
                    (lhs-number-2 (nonterminal-number grammar lhs2))
                    (item1-lhs-matches-some-kernel-item (lhs-matches-some-kernel-item lhs1))
                    (item2-lhs-matches-some-kernel-item (lhs-matches-some-kernel-item lhs2)))
               (cond
                ((and item1-lhs-matches-some-kernel-item (not item2-lhs-matches-some-kernel-item)) t)
                ((and (not item1-lhs-matches-some-kernel-item) item2-lhs-matches-some-kernel-item) nil)
                ((< lhs-number-1 lhs-number-2) t)
                ((> lhs-number-1 lhs-number-2) nil)
                (t (let ((item1-in-kernel (laitem-in-kernel? laitem1))
                         (item2-in-kernel (laitem-in-kernel? laitem2)))
                     (cond
                      ((and item1-in-kernel (not item2-in-kernel)) t)
                      ((and (not item1-in-kernel) item2-in-kernel) nil)
                      (t (dolist (state-laitem (state-laitems state) nil)
                           (cond
                            ((eq state-laitem laitem2) (return nil))
                            ((eq state-laitem laitem1) (return t))))))))))))
          (stable-sort laitems #'laitem-<))))))


(defun print-state (state &optional (stream t))
  (pprint-logical-block (stream nil)
    (format stream "S~D:" (state-number state))
    (pprint-indent :block 2 stream)
    (pprint-newline :mandatory stream)
    (pprint-logical-block (stream (laitems-sorted-nicely state))
      (do ((first t nil))
          (nil)
        (pprint-exit-if-list-exhausted)
        (unless first
          (pprint-newline :mandatory stream))
        (let ((laitem (pprint-pop)))
          (pprint-logical-block (stream nil)
            (unless (laitem-in-kernel? laitem)
              (write-string "  " stream))
            (pprint-indent :block 4 stream)
            (print-laitem laitem stream)))))
    (when (state-transitions state)
      (pprint-newline :mandatory stream)
      (pprint-logical-block (stream (collect-equivalences (state-transitions state) :test #'equal) :prefix "Transitions: ")
        (loop
          (pprint-exit-if-list-exhausted)
          (let ((transition-cons (pprint-pop)))
            (pprint-logical-block (stream nil)
              (let ((terminals (car transition-cons)))
                (if (equal terminals '(nil))
                  (write-string "any" stream)
                  (pprint-fill stream terminals nil)))
              (format stream " ~2I~_=> ")
              (print-transition (cdr transition-cons) stream))
            (format stream "   ~:_")))))
    (when (state-gotos state)
      (pprint-newline :mandatory stream)
      (pprint-logical-block (stream (state-gotos state) :prefix "Gotos: ")
        (loop
          (pprint-exit-if-list-exhausted)
          (let ((goto-cons (pprint-pop)))
            (format stream "~@<~W ~:_=> ~:_S~D~:>  ~:_" (car goto-cons) (state-number (cdr goto-cons)))))))))


(defmethod print-object ((state state) stream)
  (print-unreadable-object (state stream)
    (format stream "state S~D" (state-number state))))


;;; ------------------------------------------------------------------------------------------------------
;;; PARAMETER TREES

;;; A parameter tree describes which nonterminal parameters are generic and which are specific in all
;;; productions derived from the same nonterminal symbol.  The tree has as many levels as there are
;;; parameters for that nonterminal symbol.  A level is generic if it is generic in all applicable
;;; productions; otherwise it is specific.
;;;
;;; The parameter tree (and each of its subtrees) can have one of three forms:
;;;   (:rule <general-rule>)
;;;      if the generic nonterminal has no remaining parameters;
;;;   (:argument <argument> <parameter-subtree>)
;;;      if the first remaining nonterminal parameter is an argument in all productions seen so far;
;;;   (:attributes <argument-or-nil> (<attribute> . <parameter-subtree>) (<attribute> . <parameter-subtree>) ...)
;;;      if the first remaining nonterminal parameter can be one of several attributes in productions seen so far;
;;;      if argument-or-nil is not nil, it is an argument that includes all of the provided attributes.
;;;
;;; The parameter tree (and each of its subtrees) is mutable and updated in place.
;;;


; Create and return an initial parameter tree for the given remaining parameters
; of the general-production.
(defun make-parameter-subtree (grammar parameters general-production)
  (cond
   (parameters
    (let ((parameter (first parameters))
          (subtree (make-parameter-subtree grammar (rest parameters) general-production)))
      (if (nonterminal-argument? parameter)
        (list :argument parameter subtree)
        (list :attributes nil (cons parameter subtree)))))
   ((production? general-production)
    (list :rule (grammar-rule grammar (production-lhs general-production))))
   (t
    (assert-true (generic-production? general-production))
    (list :rule (make-generic-rule (list general-production))))))


; Create and return an initial parameter tree for the general-production.
(defun make-parameter-tree (grammar general-production)
  (make-parameter-subtree grammar
                          (general-nonterminal-parameters (general-production-lhs general-production))
                          general-production))


; The parameter subtree has kind :argument at its top level.  Convert it in place
; into kind :attributes.
(defun parameter-subtree-divide-argument (grammar parameter-subtree)
  (let ((argument (second parameter-subtree))
        (argument-subtree (third parameter-subtree)))
    (labels
      ((substitute-argument-with (attribute subtree)
         (ecase (first subtree)
           (:rule
             (let* ((general-rule (second subtree))
                    (lhs (general-rule-lhs general-rule))
                    (new-lhs (general-grammar-symbol-substitute attribute argument lhs)))
               (assert-true (generic-rule? general-rule))
               (list :rule 
                     (if (generic-nonterminal? new-lhs)
                       (generic-rule-substitute grammar attribute argument general-rule)
                       (grammar-rule grammar new-lhs)))))
           (:argument
            (list :argument
                  (second subtree)
                  (substitute-argument-with attribute (third subtree))))
           (:attributes
            (list :attributes
                  (second subtree)
                  (mapcar #'(lambda (argument-subtree-binding)
                              (cons (car argument-subtree-binding)
                                    (substitute-argument-with attribute (cdr argument-subtree-binding))))
                          (cddr subtree))))))
       
       (create-attribute-subtree-binding (attribute)
         (cons attribute (substitute-argument-with attribute argument-subtree))))
      
      (setf (first parameter-subtree) :attributes)
      (setf (cddr parameter-subtree)
            (mapcar #'create-attribute-subtree-binding
                    (grammar-parametrization-lookup-argument grammar argument))))))


; Mutate the parameter subtree to add the general-production to its rules.
; parameters are the nonterminal parameters for the remaining subtree levels.
(defun parameter-subtree-add-production (grammar parameter-subtree parameters general-production)
  (let ((lhs (general-production-lhs general-production))
        (kind (first parameter-subtree)))
    (cond
     ((eq kind :rule)
      (let ((general-rule (second parameter-subtree)))
        (cond
         (parameters
          (error "Extra nonterminal parameters ~S for ~S" parameters lhs))
         ((production? general-production)
          (assert-true (member general-production (rule-productions general-rule) :test #'eq)))
         (t
          (assert-true (generic-production? general-production))
          (assert-true (not (member general-production (generic-rule-productions general-rule) :test #'eq)))
          (setf (generic-rule-productions general-rule)
                (nconc (generic-rule-productions general-rule) (list general-production)))))))
     ((null parameters)
      (error "Missing nonterminal parameters in ~S" lhs))
     (t
      (let ((parameter (first parameters))
            (parameters-rest (rest parameters)))
        (ecase kind
          (:argument
           (let ((argument (second parameter-subtree))
                 (argument-subtree (third parameter-subtree)))
             (if (nonterminal-argument? parameter)
               (if (eq parameter argument)
                 (parameter-subtree-add-production grammar argument-subtree parameters-rest general-production)
                 (error "Nonterminal argument conflict: ~S vs. ~S" parameter argument))
               (progn
                 (parameter-subtree-divide-argument grammar parameter-subtree)
                 (parameter-subtree-add-production grammar parameter-subtree parameters general-production)))))
          (:attributes
           (let ((argument (second parameter-subtree))
                 (attribute-subtree-bindings (cddr parameter-subtree)))
             (if (nonterminal-argument? parameter)
               (let ((argument-attributes (grammar-parametrization-lookup-argument grammar parameter)))
                 (labels
                   ((add-attribute-production (attribute-subtree-binding)
                      (let ((attribute (car attribute-subtree-binding)))
                        (unless (member attribute argument-attributes :test #'eq)
                          (error "Attribute ~S is not a member of argument ~S" attribute parameter))
                        (parameter-subtree-add-production
                         grammar
                         (cdr attribute-subtree-binding)
                         parameters-rest
                         (generic-production-substitute grammar attribute parameter general-production)))))
                   (cond
                    ((null argument)
                     (mapc #'add-attribute-production attribute-subtree-bindings)
                     (setf (cddr parameter-subtree)
                           (nconc attribute-subtree-bindings
                                  (mapcan #'(lambda (attribute)
                                              (unless (assoc attribute attribute-subtree-bindings :test #'eq)
                                                (list
                                                 (cons attribute
                                                       (make-parameter-subtree
                                                        grammar
                                                        parameters-rest
                                                        (generic-production-substitute grammar attribute parameter general-production))))))
                                          argument-attributes)))
                     (setf (second parameter-subtree) parameter))
                    ((eq parameter argument)
                     (assert-true (= (length attribute-subtree-bindings) (length argument-attributes)))
                     (mapc #'add-attribute-production attribute-subtree-bindings))
                    (t (error "Nonterminal argument conflict: ~S vs. ~S" parameter argument)))))
               
               (let ((attribute-subtree-binding (assoc parameter attribute-subtree-bindings :test #'eq)))
                 (cond
                  (attribute-subtree-binding
                   (parameter-subtree-add-production grammar (cdr attribute-subtree-binding) parameters-rest general-production))
                  (argument
                   (error "Attribute ~S is not a member of argument ~S" parameter argument))
                  (t
                   (setf (cddr parameter-subtree)
                         (nconc attribute-subtree-bindings
                                (list (cons parameter
                                            (make-parameter-subtree grammar parameters-rest general-production)))))))))))))))))


; Mutate the parameter tree to add the general-production to its rules.
(defun parameter-tree-add-production (grammar parameter-tree general-production)
  (parameter-subtree-add-production grammar
                                    parameter-tree
                                    (general-nonterminal-parameters (general-production-lhs general-production))
                                    general-production)
  parameter-tree)


; Return a freshly consed list of all rules and generic-rules in the parameter tree.
(defun parameter-tree-general-rules (parameter-tree)
  (ecase (first parameter-tree)
    (:rule (list (second parameter-tree)))
    (:argument (parameter-tree-general-rules (third parameter-tree)))
    (:attributes
     (mapcan
      #'(lambda (argument-subtree-binding)
          (parameter-tree-general-rules (cdr argument-subtree-binding)))
      (cddr parameter-tree)))))


;;; ------------------------------------------------------------------------------------------------------
;;; GRAMMAR

(defstruct (grammar (:include grammar-parametrization)
                    (:constructor allocate-grammar)
                    (:copier nil)
                    (:predicate grammar?))
  (terminals nil :type simple-vector :read-only t)       ;Vector of all terminals (in order of terminal numbers)
  (nonterminals nil :type simple-vector :read-only t)    ;Vector of all nonterminals (in a depth-first order)
  (nonterminals-list nil :type list :read-only t)        ;List version of the nonterminals vector
  (terminal-variants nil :type hash-table :read-only t)  ;Hash table of terminal -> list of that terminal's variants, including the terminal itself
  (terminal-terminalsets nil :type hash-table :read-only t) ;Hash table of terminal -> terminalset of that terminal's variants, including the terminal itself
  (terminal-numbers nil :type hash-table :read-only t)   ;Hash table of terminal -> terminal number
  (terminal-actions nil :type hash-table :read-only t)   ;Hash table of terminal -> list of (action-symbol . action-function-or-nil)
  (rules nil :type hash-table :read-only t)              ;Hash table of nonterminal -> rule
  (parameter-trees nil :type hash-table :read-only t)    ;Hash table of nonterminal-symbol -> parameter-tree
  (max-production-length nil :type integer :read-only t) ;Maximum number of grammar symbols in the rhs of a production
  (general-productions nil :type hash-table :read-only t);Hash table of production-name -> general-production
  (n-productions nil :type integer :read-only t)         ;Number of productions in the grammar
  (highlights nil :type list :read-only t)               ;List of style keywords for highlighting selected productions
  ;The following fields are used for the parser.
  (items-hash nil :type (or null hash-table))            ;Hash table of (production . dot) -> item; nil for a cleaned grammar or a grammar without a parser
  (states nil :type list)                                ;List of LR(0) states (in order of state numbers)
  ;The following fields are used for the action generator.
  (action-signatures nil :type (or null hash-table)))    ;Hash table of grammar-symbol -> list of (action-symbol . type-or-type-expr)


; Return a list of the given terminal's variants, including the terminal itself.
(declaim (inline terminal-variants))
(defun terminal-variants (grammar terminal)
  (or (gethash terminal (grammar-terminal-variants grammar))
      (error "Can't find terminal ~S" terminal)))


; Return a terminalset of the given terminal's variants, including the terminal itself.
(declaim (inline terminal-terminalset))
(defun terminal-terminalset (grammar terminal)
  (or (gethash terminal (grammar-terminal-terminalsets grammar))
      (error "Can't find terminal ~S" terminal)))


; Return a rule for the given nonterminal lhs.
(defun grammar-rule (grammar lhs)
  (or (gethash lhs (grammar-rules grammar))
      (error "Can't find rule for ~S" lhs)))


; Return a list of general-rules that together form a partition of all productions whose lhs's
; are instances of the given lhs-general-nonterminal.
; The given lhs-general-nonterminal must have been given as the lhs for at least one general production
; source when constructing this grammar.
(defun grammar-general-rules (grammar lhs-general-nonterminal)
  (if (generic-nonterminal? lhs-general-nonterminal)
    (parameter-tree-general-rules (gethash (generic-nonterminal-symbol lhs-general-nonterminal) (grammar-parameter-trees grammar)))
    (list (grammar-rule grammar lhs-general-nonterminal))))


; Return the start production for the grammar.
(defun grammar-start-production (grammar)
  (assert-non-null (first (rule-productions (grammar-rule grammar *start-nonterminal*)))))


; Return the user (not internal) start-symbol of the grammar.
(defun gramar-user-start-symbol (grammar)
  (assert-non-null (first (production-rhs (grammar-start-production grammar)))))


; Return the production with the given name in the grammar.
; The returned production may be a production or a generic-production.
; Signal an error if there is no such production.
(defun grammar-general-production (grammar production-name)
  (or (gethash production-name (grammar-general-productions grammar))
      (error "Production ~S not found" production-name)))


; Call f on each production in the grammar in an unspecified order.
(defun each-grammar-production (grammar f)
  (maphash
   #'(lambda (lhs rule)
       (declare (ignore lhs))
       (mapc f (rule-productions rule)))
   (grammar-rules grammar)))


; Return the starting state for the grammar.
(defun grammar-start-state (grammar)
  (assert-non-null (first (grammar-states grammar))))


; Return the state numbered n in the grammar.
(defun grammar-state (grammar n)
  (assert-non-null (nth n (grammar-states grammar))))


; Return two values:
;   a generate terminalset G;
;   a passthrough terminalset P.
;
; G is the terminalset of all terminals x that satisfy
;   symbol ==>* x rest,
; where rest is an arbitrary string of grammar symbols.
;
; P is the terminalset of all terminals x that satisfy
;   symbol x ==> x
(defun symbol-initial-terminals (grammar symbol)
  (assert-type symbol grammar-symbol)
  (if (nonterminal? symbol)
    (let ((rule (grammar-rule grammar symbol)))
      (values (rule-initial-terminals rule) (rule-passthrough-terminals rule)))
    (values (terminal-terminalset grammar symbol) *empty-terminalset*)))


; Given an arbitrary string of grammar symbols, a list of constraints, and an initial position,
; return two values:
;   a generate terminalset G;
;   a passthrough terminalset P.
;
; G is the terminalset of all terminals x that satisfy
;   symbol-string ==>* x rest,
; where rest is an arbitrary string of grammar symbols.
;
; P is the terminalset of all terminals x that satisfy
;   symbol-string x ==> x
;
; The constraints' positions are relative to the given initial position, which specifies the position
; of the first grammar-symbol in the symbol-string.  The constraints must be listed in order of
; increasing positions.  If ignore-initial-constraints is true, ignore the constraints located at pos
; but include the constraints at subsequent positions if applicable.
(defun string-initial-terminals (grammar symbol-string constraints pos ignore-initial-constraints)
  (do ((symbol-string symbol-string (cdr symbol-string))
       (pos pos (1+ pos))
       (initial-terminals *empty-terminalset*)
       (passthrough-terminals *full-terminalset*))
      ((terminalset-empty? passthrough-terminals) (values initial-terminals *empty-terminalset*))
    (if ignore-initial-constraints
      (setq ignore-initial-constraints nil)
      (dolist (constraint constraints)
        (when (= (constraint-pos constraint) pos)
          (terminalset-intersection-f passthrough-terminals (constraint-terminalset constraint)))))
    (if symbol-string
      (multiple-value-bind (generate passthrough) (symbol-initial-terminals grammar (first symbol-string))
        (terminalset-union-f initial-terminals (terminalset-intersection passthrough-terminals generate))
        (terminalset-intersection-f passthrough-terminals passthrough))
      (return (values initial-terminals passthrough-terminals)))))


; Intern attributed or generic nonterminals in the production's lhs and rhs.  Replace
; (:- <terminal> ... <terminal>) or (:-- (<grammar-symbol> ... <grammar-symbol>) <terminal> ... <terminal>)
; sublists in the rhs with lookahead-constraints and put these, in order, after the fourth element of
; the returned list.  Also replace variant constraint symbols with variant-constraints.
; The variant-constraint-names parameter should be a list of possible variant constraint symbols.
; Return the resulting production source.
(defun intern-production-source (grammar-parametrization variant-constraint-names highlights production-source)
  (assert-type production-source (tuple (or user-nonterminal cons) (list (or user-grammar-symbol cons)) identifier t))
  (let ((production-lhs-source (first production-source))
        (production-rhs-source (second production-source))
        (production-name (third production-source))
        (production-highlight (fourth production-source)))
    (unless (or (null production-highlight) (member production-highlight highlights :test #'eq))
      (error "Bad highlight in rule: ~S; allowed highlights are ~S" production-source highlights))
    (if (or (consp production-lhs-source) (some #'consp production-rhs-source) (intersection variant-constraint-names production-rhs-source))
      (multiple-value-bind (lhs-nonterminal lhs-arguments) (grammar-parametrization-intern grammar-parametrization production-lhs-source)
        (let ((rhs nil)
              (constraints nil)
              (pos 0))
          (dolist (component-source production-rhs-source)
            (cond
             ((and (consp component-source) (eq (first component-source) :-))
              (let ((lookaheads (rest component-source)))
                (push
                 (make-lookahead-constraint pos (assert-non-null lookaheads) lookaheads)
                 constraints)))
             ((and (consp component-source) (eq (first component-source) :--))
              (let ((lookaheads (rest component-source)))
                (push
                 (make-lookahead-constraint pos (assert-non-null (rest lookaheads)) (assert-non-null (first lookaheads)))
                 constraints)))
             ((member component-source variant-constraint-names)
              (push (make-variant-constraint pos component-source) constraints))
             (t
              (push (grammar-parametrization-intern grammar-parametrization component-source lhs-arguments) rhs)
              (incf pos))))
          (list* lhs-nonterminal (nreverse rhs) production-name production-highlight (nreverse constraints))))
      production-source)))


; Make a grammar structure out of a grammar in source form.
; A grammar-source is a list of productions; each production is a list of:
;   a nonterminal A (the lhs);
;   a list of grammar symbols forming A's expansion (the rhs);
;   a production name;
;   a highlight keyword or nil.
; Nonterminals in the lhs and rhs can be parametrized; in this case such a nonterminal
; is represented by a list whose first element is the name and the remaining elements are
; the arguments or attributes.  Any nonterminal argument in the rhs must also be an argument
; of the lhs nonterminal.  The lhs nonterminal must not have duplicate arguments.  The lhs
; nonterminal can have attributes, thereby designating a specialization instead of a fully
; generic production.
;
; variant-constraint-names should be a list of keywords that are variant-constraints instead of nonterminals
;
; A variant-generator is either nil or a function that takes a terminal and outputs a list of its variants.
; Each variant is specified as a cons of:
;   A variant terminal (may be eql to the original terminal; this lets one add constraints that exclude the original terminal)
;   A list of names of variant constraints which exclude this variant terminal
;
; The rhs can also contain lookahead constraints of the form
;   (:- <terminal> ... <terminal>) 
; which indicate that the following terminal must not be one of the listed terminals.  The form
;   (:-- (<grammar-symbol> ... <grammar-symbol>) <terminal> ... <terminal>)
; does the same thing except that it prints the grammar-symbols instead of the terminals when
; the production is printed.  The form
;   <constraint>
; where <constraint> is a member of the variant-constraint-names list also represents a constraint on the following
; terminal; that constraint excludes all variants that were returned by variant-generator along with this constraint.
;
; excluded-nonterminals is a list of nonterminals not used in the grammar.  Productions,
; including productions expanded from generic productions, that have one of these nonterminals
; on the lhs are ignored.
;
; highlights is a list of keywords that may be used to highlight specific rules or productions.  Each production that includes
; a highlight as its fourth element will be rendered using the markup style specified by highlight.
(defun make-grammar (grammar-parametrization start-symbol grammar-source &key variant-constraint-names variant-generator excluded-nonterminals highlights)
  (assert-type highlights (list keyword))
  (let ((variant-constraint-forbid-lists (mapcar #'list variant-constraint-names))
        (interned-grammar-source
         (mapcar #'(lambda (production-source)
                     (intern-production-source grammar-parametrization variant-constraint-names highlights production-source))
                 grammar-source))
        (rules (make-hash-table :test #'eq))
        (terminals-hash (make-hash-table :test *grammar-symbol-=*))
        (terminal-variants (make-hash-table :test *grammar-symbol-=*))
        (general-productions (make-hash-table :test #'equal))
        (production-number 0)
        (max-production-length 1)
        (excluded-nonterminals-hash (make-hash-table :test *grammar-symbol-=*))
        (constraints nil))
    
    ;Set up excluded-nonterminals-hash.  The values of the hash table are either :seen or :unseen
    ;depending on whether a production with the particular nonterminal has been seen yet.
    (dolist (excluded-nonterminal excluded-nonterminals)
      (setf (gethash (grammar-parametrization-intern grammar-parametrization excluded-nonterminal nil) excluded-nonterminals-hash)
            :unseen))
    
    ;Create the starting production:  *start-nonterminal* ==> start-symbol
    (setf (gethash *start-nonterminal* rules)
          (list (make-production *start-nonterminal* (list start-symbol) nil nil nil 1 0)))
    
    ;Create the rest of the productions.
    (flet
      ((create-production (lhs rhs constraints name highlight)
         (let ((production (make-production lhs rhs constraints name highlight (length rhs) (incf production-number))))
           (push production (gethash lhs rules))
           (dolist (rhs-terminal (production-terminals production))
             (setf (gethash rhs-terminal terminals-hash) t))
           production))
       
       (nonterminal-excluded (nonterminal)
         (and (gethash nonterminal excluded-nonterminals-hash)
              (setf (gethash nonterminal excluded-nonterminals-hash) :seen))))
      
      (dolist (production-source interned-grammar-source)
        (let* ((production-lhs (first production-source))
               (production-rhs (second production-source))
               (production-name (third production-source))
               (production-highlight (fourth production-source))
               (production-constraints (cddddr production-source))
               (lhs-arguments (general-grammar-symbol-arguments production-lhs)))
          (setq max-production-length (max max-production-length (length production-rhs)))
          (when (gethash production-name general-productions)
            (error "Duplicate production name ~S" production-name))
          (if lhs-arguments
            (let ((productions nil))
              (grammar-parametrization-each-permutation
               grammar-parametrization
               #'(lambda (bound-argument-alist)
                   (let ((instantiated-lhs (instantiate-general-grammar-symbol bound-argument-alist production-lhs)))
                     (unless (nonterminal-excluded instantiated-lhs)
                       (push (create-production
                              instantiated-lhs
                              (mapcar #'(lambda (general-grammar-symbol)
                                          (instantiate-general-grammar-symbol bound-argument-alist general-grammar-symbol))
                                      production-rhs)
                              production-constraints
                              production-name
                              production-highlight)
                             productions))))
               lhs-arguments)
              (when productions
                (setf (gethash production-name general-productions)
                      (make-generic-production production-lhs production-rhs production-constraints production-name production-highlight (nreverse productions)))))
            
            (unless (nonterminal-excluded production-lhs)
              (setf (gethash production-name general-productions)
                    (create-production production-lhs production-rhs production-constraints production-name production-highlight)))))))
    
    
    (when variant-generator
      (dolist (terminal (hash-table-keys terminals-hash))
        (dolist (variant-and-constraints (funcall variant-generator terminal))
          (let ((variant (first variant-and-constraints)))
            (unless (grammar-symbol-= terminal variant)
              (push variant (gethash terminal terminal-variants))
              (setf (gethash variant terminals-hash) t))
            (dolist (constraint (rest variant-and-constraints))
              (push variant (cdr (assoc constraint variant-constraint-forbid-lists))))))))
    
    
    ;Change all values of the rules hash table to contain rule structures
    ;instead of mere lists of rules.  Also check that all referenced nonterminals
    ;have been defined.
    (maphash
     #'(lambda (rule-lhs rule-productions)
         (dolist (rule-production rule-productions)
           (dolist (grammar-symbol (production-rhs rule-production))
             (when (nonterminal? grammar-symbol)
               (unless (gethash grammar-symbol rules)
                 (error "Nonterminal ~S used but not defined" grammar-symbol))))
           (dolist (constraint (production-constraints rule-production))
             (push constraint constraints)
             (when (lookahead-constraint? constraint)
               (dolist (lookahead-terminal (lookahead-constraint-forbidden-terminals constraint))
                 (unless (gethash lookahead-terminal terminals-hash)
                   (warn "Lookahead terminal ~S not used in main grammar" lookahead-terminal)
                   (setf (gethash lookahead-terminal terminals-hash) t))))))
         (setf (gethash rule-lhs rules)
               (make-rule (nreverse rule-productions))))
     rules)
    
    ;Check that all excluded nonterminals have been seen.
    (maphash
     #'(lambda (excluded-nonterminal seen)
         (unless (eq seen :seen)
           (warn "Nonterminal ~S declared excluded but not defined" excluded-nonterminal)))
     excluded-nonterminals-hash)
    
    (let ((nonterminals-list (depth-first-search
                              *grammar-symbol-=*
                              #'(lambda (nonterminal) (rule-nonterminals (gethash nonterminal rules)))
                              *start-nonterminal*)))
      (when (/= (length nonterminals-list) (hash-table-count rules))
        (let ((dead-nonterminals (set-difference (hash-table-keys rules) nonterminals-list :test *grammar-symbol-=*)))
          (warn "The following nonterminals are not reachable from start: ~S" dead-nonterminals)
          (setq nonterminals-list (nconc nonterminals-list dead-nonterminals))))
      
      (let ((terminals (coerce (cons *end-marker* (sorted-hash-table-keys terminals-hash))
                               'simple-vector))
            (terminal-terminalsets (make-hash-table :test *grammar-symbol-=*))
            (nonterminals (coerce nonterminals-list 'simple-vector)))
        (dotimes (n (length terminals))
          (let ((terminal (svref terminals n)))
            (setf (gethash terminal terminals-hash) n)
            (pushnew terminal (gethash terminal terminal-variants))))
        (dotimes (n (length nonterminals))
          (setf (rule-number (gethash (svref nonterminals n) rules)) n))
        (let ((grammar (allocate-grammar
                        :argument-attributes (grammar-parametrization-argument-attributes grammar-parametrization)
                        :terminals terminals
                        :nonterminals nonterminals
                        :nonterminals-list nonterminals-list
                        :terminal-variants terminal-variants
                        :terminal-terminalsets terminal-terminalsets
                        :terminal-numbers terminals-hash
                        :terminal-actions (make-hash-table :test *grammar-symbol-=*)
                        :rules rules
                        :parameter-trees (make-hash-table :test *grammar-symbol-=*)
                        :max-production-length max-production-length
                        :general-productions general-productions
                        :n-productions production-number
                        :highlights highlights)))
          
          ;Compute the terminalsets in the terminal-terminalsets.
          (dotimes (n (length terminals))
            (let ((terminal (svref terminals n))
                  (terminalset *empty-terminalset*))
              (dolist (variant (terminal-variants grammar terminal))
                (terminalset-union-f terminalset (make-terminalset grammar variant)))
              (setf (gethash terminal terminal-terminalsets) terminalset)))
          
          ;Replace the cdr of each element of variant-constraint-forbid-lists with a terminalset of all
          ;terminals not forbidden by that constraint.
          (dolist (constraint-and-forbidden-variants variant-constraint-forbid-lists)
            (let ((terminalset *full-terminalset*))
              (dolist (variant (rest constraint-and-forbidden-variants))
                (terminalset-difference-f terminalset (ash 1 (gethash variant terminals-hash)))
                (setf (rest constraint-and-forbidden-variants) terminalset))))
          
          ;Compute the terminalsets in the constraints.
          (dolist (constraint constraints)
            (cond
             ((lookahead-constraint? constraint)
              (let ((terminalset *full-terminalset*))
                (dolist (forbidden-terminal (lookahead-constraint-forbidden-terminals constraint))
                  (terminalset-difference-f terminalset (terminal-terminalset grammar forbidden-terminal)))
                (setf (constraint-terminalset constraint) terminalset)))
             ((variant-constraint? constraint)
              (setf (constraint-terminalset constraint) (assert-non-null (cdr (assoc (variant-constraint-name constraint)
                                                                                     variant-constraint-forbid-lists)))))
             (t (error "Unknown constraint ~S" constraint))))
          
          ;Compute the values of passthrough-terminals and initial-terminals in each rule.
          (do ((changed t))
              ((not changed))
            (setq changed nil)
            (dolist (nonterminal (grammar-nonterminals-list grammar))
              (let ((rule (grammar-rule grammar nonterminal))
                    (new-initial-terminals *empty-terminalset*)
                    (new-passthrough-terminals *empty-terminalset*))
                (dolist (production (rule-productions rule))
                  (multiple-value-bind (production-initial-terminals production-passthrough-terminals)
                                       (string-initial-terminals grammar (production-rhs production) (production-constraints production) 0 nil)
                    (terminalset-union-f new-initial-terminals production-initial-terminals)
                    (terminalset-union-f new-passthrough-terminals production-passthrough-terminals)))
                (unless (terminalset-= new-initial-terminals (rule-initial-terminals rule))
                  (assert-true (terminalset-<= (rule-initial-terminals rule) new-initial-terminals))
                  (setf (rule-initial-terminals rule) new-initial-terminals)
                  (setq changed t))
                (unless (terminalset-= new-passthrough-terminals (rule-passthrough-terminals rule))
                  (assert-true (terminalset-<= (rule-passthrough-terminals rule) new-passthrough-terminals))
                  (setf (rule-passthrough-terminals rule) new-passthrough-terminals)
                  (setq changed t)))))
          
          ;Compute the parameter-trees entries.
          (let ((parameter-trees (grammar-parameter-trees grammar)))
            (dolist (production-source interned-grammar-source)
              (let ((general-production (gethash (third production-source) general-productions)))
                (let ((lhs-general-nonterminal (general-production-lhs general-production)))
                  (when (or (generic-nonterminal? lhs-general-nonterminal)
                            (attributed-nonterminal? lhs-general-nonterminal))
                    (let* ((lhs-symbol (general-grammar-symbol-symbol lhs-general-nonterminal))
                           (parameter-tree (gethash lhs-symbol parameter-trees)))
                      (if parameter-tree
                        (parameter-tree-add-production grammar parameter-tree general-production)
                        (setf (gethash lhs-symbol parameter-trees)
                              (make-parameter-tree grammar general-production)))))))))
          grammar)))))


(defvar *name-print-column* 70)

; Print the grammar nicely.
(defun print-grammar (grammar &optional (stream t) &key (details t))
  (labels
    ((print-production-number (n)
       (format nil "P~D" n)))
    (let ((production-number-width (length (print-production-number (grammar-n-productions grammar)))))
      (pprint-logical-block (stream nil)
        (format stream "Terminals: ~@_~<~@{~W ~:_~}~:>~:@_" (coerce (grammar-terminals grammar) 'list))
        (when details
          (format stream "Nonterminals: ~@_~<~@{~W ~:_~}~:>~:@_" (grammar-nonterminals-list grammar)))
        
        ;Print the rules.
        (pprint-logical-block (stream (grammar-nonterminals-list grammar))
          (pprint-exit-if-list-exhausted)
          (format stream "Rules:")
          (pprint-indent :block 2 stream)
          (pprint-newline :mandatory stream)
          (loop
            (let* ((rule-lhs (pprint-pop))
                   (rule (grammar-rule grammar rule-lhs)))
              (pprint-logical-block (stream nil)
                (pprint-logical-block (stream (rule-productions rule) :prefix (format nil "~W " rule-lhs))
                  (pprint-exit-if-list-exhausted)
                  (format stream "-> ")
                  (loop
                    (let ((production (pprint-pop)))
                      (format stream "~@<~:[<epsilon> ~;~:*~{~W ~:_~}~] ~_~vT~vA [~W]~:>"
                              (general-production-rhs-components production) *name-print-column*
                              production-number-width (print-production-number (production-number production))
                              (production-name production)))
                    (pprint-exit-if-list-exhausted)
                    (format stream "~:@_ | ")))
                (pprint-indent :block 2 stream)
                (when details
                  (format stream "~:@_Initial terminals: ~@_")
                  (pprint-logical-block (stream nil)
                    (unless (terminalset-empty? (rule-passthrough-terminals rule))
                      (format stream "<epsilon> ~:_"))
                    (print-terminalset grammar (rule-initial-terminals rule) stream))))
              (pprint-newline :mandatory stream))
            (pprint-exit-if-list-exhausted)
            (pprint-newline :mandatory stream)))
        
        ;Print the states.
        (pprint-newline :mandatory stream)
        (pprint-logical-block (stream (grammar-states grammar))
          (pprint-exit-if-list-exhausted)
          (unless (grammar-items-hash grammar)
            (error "Can't print a cleaned grammar's states"))
          (format stream "States:")
          (pprint-indent :block 2 stream)
          (pprint-newline :mandatory stream)
          (loop
            (print-state (pprint-pop) stream)
            (pprint-newline :mandatory stream)
            (pprint-exit-if-list-exhausted)
            (pprint-newline :mandatory stream))))
      (pprint-newline :mandatory stream))))


(defmethod print-object ((grammar grammar) stream)
  (print-unreadable-object (grammar stream :identity t)
    (write-string "grammar" stream)))


; Emit markup paragraphs for the grammar.
(defun depict-grammar (markup-stream grammar)
  (depict-paragraph (markup-stream :body-text)
    (depict markup-stream "Start nonterminal: ")
    (depict-general-nonterminal markup-stream (gramar-user-start-symbol grammar) :reference))
  (dolist (nonterminal (grammar-nonterminals-list grammar))
    (unless (grammar-symbol-= nonterminal *start-nonterminal*)
      (depict-general-rule markup-stream (grammar-rule grammar nonterminal) (grammar-highlights grammar)))))


; Return a list of nontrivial sets of states with the same kernels.
(defun grammar-kernel-sets (grammar)
  (let ((states-hash (make-hash-table :test #'equal))
        (equivalences nil))
    (dolist (state (grammar-states grammar))
      (push state (gethash (state-kernel state) states-hash)))
    (maphash #'(lambda (kernel states)
                 (declare (ignore kernel))
                 (unless (= (length states) 1)
                   (push (nreverse states) equivalences)))
             states-hash)
    (sort equivalences #'< :key #'(lambda (equivalence)
                                    (state-number (first equivalence))))))

; Call f on each state in the grammar, in order of state numbers.
; f should take two parameters:
;   a state;
;   a hash table of terminal -> transition
(defun all-state-transitions (f grammar)
  (let ((transitions-hash (make-hash-table :test *grammar-symbol-=*)))
    (dolist (state (grammar-states grammar))
      (dolist (transition-pair (state-transitions state))
        (setf (gethash (car transition-pair) transitions-hash) (cdr transition-pair)))
      (funcall f state transitions-hash)
      (clrhash transitions-hash))))


;;; ------------------------------------------------------------------------------------------------------
;;; YACC OUTPUT


; Print the nonterminal in a yacc-like form.
(defun yacc-print-nonterminal (nonterminal &optional (stream t))
  (cond
   ((keywordp nonterminal)
    (write-string (symbol-upper-mixed-case-name nonterminal) stream))
   ((attributed-nonterminal? nonterminal)
    (write-string (symbol-upper-mixed-case-name (attributed-nonterminal-symbol nonterminal)) stream)
    (dolist (attribute (attributed-nonterminal-attributes nonterminal))
      (write-char #\_ stream)
      (write-string (symbol-lower-mixed-case-name attribute) stream)))
   (t (error "Bad nonterminal ~S" nonterminal))))


; Print the grammar-symbol in a yacc-like form.
(defun yacc-print-grammar-symbol (grammar-symbol &optional (stream t))
  (cond
   ((nonterminal? grammar-symbol)
    (yacc-print-nonterminal grammar-symbol stream))
   ((and grammar-symbol (symbolp grammar-symbol))
    (let ((name (symbol-name grammar-symbol)))
      (if (and (> (length name) 0) (char= (char name 0) #\$))
        (write-string (string-upcase name :start 1) stream)
        (format stream "\"~A\"" (string-downcase name)))))
   (t (error "Don't know how to emit markup for terminal ~S" grammar-symbol))))

#|
; Print the grammar in a yacc-like form.
(defun yacc-print-grammar (grammar &optional (stream t))
|#
