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
;;; LALR(1) and LR(1) grammar generator
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;


;;; ------------------------------------------------------------------------------------------------------
;;; TERMINALSETS

;;; A set of terminals is a bitset represented by an integer, indexed by terminal numbers.

(deftype terminalset () 'integer)

(defconstant *empty-terminalset* 0)

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


; Merge two sets of lookaheads sorted by increasing terminal numbers, eliminating
; duplicates.  Return the combined set.
(declaim (inline terminalset-union))
(defun terminalset-union (terminalset1 terminalset2)
  (logior terminalset1 terminalset2))


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


; Call f on every terminal in the terminalset in reverse order.
(defun map-terminalset-reverse (f grammar terminalset)
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
  (pprint-fill stream (terminalset-list grammar terminalset) nil))


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
(defstruct (action (:constructor make-action (expr))
                   (:predicate action?))
  (expr nil :read-only t)                     ;The unparsed source expression that defines the action
  (code nil))                                 ;The generated lisp source code that performs the action


;;; ------------------------------------------------------------------------------------------------------
;;; GENERALIZED PRODUCTIONS

;;; A general-production is a common base class for the production and generic-production classes.
(defstruct (general-production (:constructor nil) (:copier nil) (:predicate general-production?))
  (lhs nil :type general-nonterminal :read-only t) ;The general-nonterminal on the left-hand side of this general-production
  (rhs nil :type list :read-only t)                ;List of general grammar symbols to which that general-nonterminal expands
  (name nil :read-only t))                         ;This general-production's name that will be used to name the parse tree node


; If general-production is a generic-production, return its list of productions;
; if general-production is a production, return it in a one-element list.
(defun general-production-productions (general-production)
  (if (production? general-production)
    (list general-production)
    (generic-production-productions general-production)))


; Emit a markup paragraph for the left-hand-side of a general production.
(defun depict-general-production-lhs (markup-stream lhs-general-nonterminal)
  (depict-paragraph (markup-stream ':grammar-lhs)
    (depict-general-nonterminal markup-stream lhs-general-nonterminal)
    (depict markup-stream " " ':derives-10)))


; Emit a markup paragraph for the right-hand-side of a general production.
; first is true if this is the first production in a rule.
; last is true if this is the last production in a rule.
(defun depict-general-production-rhs (markup-stream general-production first last)
  (depict-paragraph (markup-stream (if last ':grammar-rhs-last ':grammar-rhs))
    (if first
      (depict markup-stream ':tab3)
      (depict markup-stream "|" ':tab2))
    (let ((rhs (general-production-rhs general-production)))
      (depict-list markup-stream #'depict-general-grammar-symbol rhs
                   :separator " "
                   :empty '(:left-angle-quote "empty" :right-angle-quote)))))


; Emit the general production, including both its left and right-hand sides.
; Include serial number subscripts on all rhs grammar symbols that both
;    appear more than once in the rhs or appear in the lhs; and
;    have symbols that are present in the symbols-with-subscripts list.
(defun depict-general-production (markup-stream general-production &optional symbols-with-subscripts)
  (let ((lhs (general-production-lhs general-production))
        (rhs (general-production-rhs general-production)))
    (depict-general-nonterminal markup-stream lhs)
    (depict markup-stream " " ':derives-10)
    (if rhs
      (let ((counts-hash (make-hash-table :test *grammar-symbol-=*)))
        (when symbols-with-subscripts
          (dolist (symbol symbols-with-subscripts)
            (setf (gethash symbol counts-hash) 0))
          (dolist (general-grammar-symbol (cons lhs rhs))
            (let ((symbol (general-grammar-symbol-symbol general-grammar-symbol)))
              (when (gethash symbol counts-hash)
                (incf (gethash symbol counts-hash)))))
          (maphash #'(lambda (symbol count)
                       (assert-true (> count 0))
                       (if (> count 1)
                         (setf (gethash symbol counts-hash) 0)
                         (remhash symbol counts-hash)))
                   counts-hash))
        (dolist (general-grammar-symbol rhs)
          (let* ((symbol (general-grammar-symbol-symbol general-grammar-symbol))
                 (subscript (and (gethash symbol counts-hash) (incf (gethash symbol counts-hash)))))
            (depict-space markup-stream)
            (depict-general-grammar-symbol markup-stream general-grammar-symbol subscript))))
      (depict markup-stream " " ':left-angle-quote "empty" :right-angle-quote))))


;;; ------------------------------------------------------------------------------------------------------
;;; PRODUCTIONS

;;; A production describes the expansion of a nonterminal (the lhs) into
;;; a string of zero or more grammar symbols (the rhs).
;;; Each production has a unique number.  Earlier productions have smaller numbers.
;;; There is exactly one production structure for a given production, so eq can be
;;; used to test for production equality.
;;;
;;; The evaluator is a lisp form that evaluates to a function f that takes one argument --
;;; the old state of the parser's value stack -- and returns the new state of that stack.
(defstruct (production (:include general-production (lhs nil :type nonterminal :read-only t))
                       (:constructor make-production (lhs rhs name rhs-length number))
                       (:copier nil) (:predicate production?))
  (rhs-length nil :type integer :read-only t) ;Number of grammar symbols in the rhs
  (number nil :type integer :read-only t)     ;This production's serial number
  ;The following fields are used for the action generator.
  (actions nil :type list)                    ;List of (action-symbol . action-or-nil) in the same order as the action-symbols
  ;                                           ; are listed in the grammar's action-signatures hash table for this lhs
  (n-action-args nil :type (or null integer)) ;Total size of the action-signatures of all grammar symbols in the rhs
  (evaluator-code nil)                        ;The lisp evaluator's source code
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
          (list (production-lhs production) (production-rhs production))
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
                               (:constructor make-generic-production (lhs rhs name productions))
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
       (mapcar #'(lambda (rhs-general-grammar-symbol)
                   (general-grammar-symbol-substitute attribute argument rhs-general-grammar-symbol))
               (generic-production-rhs generic-production))
       (generic-production-name generic-production)
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


; Emit markup paragraphs for the grammar general rule.
; If the rule is short enough (only one production), emit the rule on one line.
(defun depict-general-rule (markup-stream general-rule)
  (depict-block-style (markup-stream ':grammar-rule)
    (let ((general-productions (general-rule-productions general-rule)))
      (assert-true general-productions)
      (if (cdr general-productions)
        (labels
          ((emit-general-productions (general-productions first)
             (let ((general-production (first general-productions))
                   (rest (rest general-productions)))
               (depict-general-production-rhs markup-stream general-production first (endp rest))
               (when rest
                 (emit-general-productions rest nil)))))
          (depict-general-production-lhs markup-stream (general-rule-lhs general-rule))
          (emit-general-productions general-productions t))
        (depict-paragraph (markup-stream ':grammar-lhs-last)
          (depict-general-production markup-stream (first general-productions)))))))


;;; ------------------------------------------------------------------------------------------------------
;;; RULES

;;; A rule is the set of all productions with the same lhs nonterminal.
;;; There is exactly one rule structure for a given nonterminal lhs, so eq can be
;;; used to test for rule equality.
(defstruct (rule (:include general-rule (productions nil :type list :read-only t))
                 (:constructor make-rule (productions))
                 (:copier nil)
                 (:predicate rule?))
  ;productions                                ;The list of all productions for this rule's nonterminal lhs
  (number nil :type (or null integer))        ;This nonterminal's serial number
  (derives-epsilon nil :type bool)            ;True if some direct or indirect expansion of this nonterminal can return epsilon
  (initial-terminals *empty-terminalset* :type terminalset)) ;Set of all terminals that can begin some expansion of this nonterminal


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


(defun print-item (item &optional (stream t))
  (let ((production (item-production item)))
    (format stream "~W -> ~:_" (production-lhs production))
    (pprint-logical-block (stream (production-rhs production))
      (do ((n (item-dot item) (1- n))
           (first t))
          ()
        (when (zerop n)
          (if first
            (setq first nil)
            (format stream " ~:_"))
          (write-char #\. stream))
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
(defstruct (laitem (:constructor allocate-laitem (grammar item lookaheads))
                   (:copier nil)
                   (:predicate laitem?))
  (grammar nil :type grammar :read-only t) ;The grammar to which this laitem belongs
  (item nil :type item :read-only t)       ;The item to which this laitem corresponds
  (lookaheads nil :type terminalset)       ;Set of lookahead terminals
  (propagates nil :type list))             ;List of laitems to which lookaheads propagate from this laitem (see note below)
;When parsing a LALR(1) grammar, propagates is the list of all laitems (in this and other states)
;to which lookaheads propagate from this laitem.
;When parsing a LR(1) grammar, propagates is the list of all laitems to which lookaheads propagate from this laitem
;without following a shift transition.  Such laitems must necessarily be in the same state.  In the LR(1) case each
;laitem listed in the propagates list must come after this laitem in this state's laitems list.


(defvar *lookahead-print-column* 70)

(defun print-laitem (laitem &optional (stream t))
  (print-item (laitem-item laitem) stream)
  (format stream " ~vT~_" *lookahead-print-column*)
  (pprint-logical-block (stream nil :prefix "{" :suffix "}")
    (print-terminalset (laitem-grammar laitem) (laitem-lookaheads laitem) stream)))

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
  (kernel nil :type list :read-only t)     ;List of kernel items in order of increasing item-number values
  (laitems nil :type list :read-only t)    ;List of laitems (topologically sorted by the propagates relation when parsing LR(1))
  (transitions nil :type list)             ;List of (terminal . transition)
  (gotos nil :type list))                  ;List of (nonterminal . state)


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
             (member lhs-nonterminal (state-kernel state) :test *grammar-symbol-=* :key #'item-lhs))
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
          ()
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
              (pprint-fill stream (car transition-cons) nil)
              (format stream " ~2I~_=> " (car transition-cons))
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
        (list ':argument parameter subtree)
        (list ':attributes nil (cons parameter subtree)))))
   ((production? general-production)
    (list ':rule (grammar-rule grammar (production-lhs general-production))))
   (t
    (assert-true (generic-production? general-production))
    (list ':rule (make-generic-rule (list general-production))))))


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
              (list ':rule 
                    (if (generic-nonterminal? new-lhs)
                      (generic-rule-substitute grammar attribute argument general-rule)
                      (grammar-rule grammar new-lhs)))))
           (:argument
            (list ':argument
                  (second subtree)
                  (substitute-argument-with attribute (third subtree))))
           (:attributes
            (list ':attributes
                  (second subtree)
                  (mapcar #'(lambda (argument-subtree-binding)
                              (cons (car argument-subtree-binding)
                                    (substitute-argument-with attribute (cdr argument-subtree-binding))))
                          (cddr subtree))))))
       
       (create-attribute-subtree-binding (attribute)
         (cons attribute (substitute-argument-with attribute argument-subtree))))
      
      (setf (first parameter-subtree) ':attributes)
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
  (terminal-numbers nil :type hash-table :read-only t)   ;Hash table of terminal -> terminal number
  (terminal-actions nil :type hash-table :read-only t)   ;Hash table of terminal -> list of (action-symbol . action-function-or-nil)
  (rules nil :type hash-table :read-only t)              ;Hash table of nonterminal -> rule
  (parameter-trees nil :type hash-table :read-only t)    ;Hash table of nonterminal-symbol -> parameter-tree
  (max-production-length nil :type integer :read-only t) ;Maximum number of grammar symbols on the rhs of a production
  (general-productions nil :type hash-table :read-only t);Hash table of production-name -> general-production
  (n-productions nil :type integer :read-only t)         ;Number of productions in the grammar
  ;The following fields are used for the parser.
  (items-hash nil :type hash-table :read-only t)         ;Hash table of (production . dot) -> item
  (states nil :type list)                                ;List of LR(0) states (in order of state numbers)
  ;The following fields are used for the action generator.
  (action-signatures nil :type (or null hash-table)))    ;Hash table of grammar-symbol -> list of (action-symbol . type-or-type-expr)


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


; Return true if symbol ==>* epsilon.
(defun symbol-derives-epsilon (grammar symbol)
  (assert-type symbol grammar-symbol)
  (and (nonterminal? symbol)
       (rule-derives-epsilon (grammar-rule grammar symbol))))


; Return the terminalset of all terminals a that satisfy
;   symbol ==>* a rest,
; where rest is an arbitrary string of grammar symbols.
(defun symbol-initial-terminals (grammar symbol)
  (assert-type symbol grammar-symbol)
  (if (nonterminal? symbol)
    (rule-initial-terminals (grammar-rule grammar symbol))
    (make-terminalset grammar symbol)))


; Given symbol-string, an arbitrary string of grammar symbols,
; return two values:  a terminalset S and a boolean B.
; S is the terminalset of all terminals a that satisfy
;   symbol-string ==>* a rest,
; where rest is an arbitrary string of grammar symbols.
; B is true if symbol-string ==>* epsilon.
(defun string-initial-terminals (grammar symbol-string)
  (let ((initial-terminals *empty-terminalset*)
        (derives-epsilon nil))
    (dolist (element symbol-string (setq derives-epsilon t))
      (setq initial-terminals (terminalset-union initial-terminals (symbol-initial-terminals grammar element)))
      (unless (symbol-derives-epsilon grammar element)
        (return)))
    (values initial-terminals derives-epsilon)))


; Intern attributed or generic nonterminals in the production's lhs and rhs.  Return the
; resulting production source.
(defun intern-production-source (grammar-parametrization production-source)
  (assert-type production-source (tuple (or user-nonterminal cons) (list (or user-grammar-symbol cons)) identifier))
  (let ((production-lhs-source (first production-source))
        (production-rhs-source (second production-source))
        (production-name (third production-source)))
    (if (or (consp production-lhs-source) (some #'consp production-rhs-source))
      (multiple-value-bind (lhs-nonterminal lhs-arguments) (grammar-parametrization-intern grammar-parametrization production-lhs-source)
        (list lhs-nonterminal
              (mapcar #'(lambda (grammar-symbol-source)
                          (grammar-parametrization-intern grammar-parametrization grammar-symbol-source lhs-arguments))
                      production-rhs-source)
              production-name))
      production-source)))


; Make a grammar structure out of a grammar in source form.
; A grammar-source is a list of productions; each production is a list of:
;   a nonterminal A (the lhs);
;   a list of grammar symbols forming A's expansion (the rhs);
;   a production name.
; Nonterminals in the lhs and rhs can be parametrized; in this case such a nonterminal
; is represented by a list whose first element is the name and the remaining elements are
; the arguments or attributes.  Any nonterminal argument in the rhs must also be an argument
; of the lhs nonterminal.  The lhs nonterminal must not have duplicate arguments.  The lhs
; nonterminal can have attributes, thereby designating a specialization instead of a fully
; generic production.
;
; excluded-nonterminals-source is a list of nonterminals not used in the grammar.  Productions,
; including productions expanded from generic productions, that have one of these nonterminals
; on the lhs are ignored.
(defun make-grammar (grammar-parametrization start-symbol grammar-source &optional excluded-nonterminals-source)
  (let ((interned-grammar-source
         (mapcar #'(lambda (production-source)
                     (intern-production-source grammar-parametrization production-source))
                 grammar-source))
        (rules (make-hash-table :test #'eq))
        (terminals-hash (make-hash-table :test *grammar-symbol-=*))
        (general-productions (make-hash-table :test #'equal))
        (production-number 0)
        (max-production-length 1)
        (excluded-nonterminals-hash (make-hash-table :test *grammar-symbol-=*)))
    
    ;Set up excluded-nonterminals-hash.  The values of the hash table are either :seen or :unseen
    ;depending on whether a production with the particular nonterminal has been seen yet.
    (dolist (excluded-nonterminal-source excluded-nonterminals-source)
      (setf (gethash (grammar-parametrization-intern grammar-parametrization excluded-nonterminal-source nil) excluded-nonterminals-hash)
            :unseen))
    
    ;Create the starting production:  *start-nonterminal* ==> start-symbol
    (setf (gethash *start-nonterminal* rules)
          (list (make-production *start-nonterminal* (list start-symbol) nil 1 0)))
    
    ;Create the rest of the productions.
    (flet
      ((create-production (lhs rhs name)
         (let ((production (make-production lhs rhs name (length rhs) (incf production-number))))
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
                              production-name)
                             productions))))
               lhs-arguments)
              (when productions
                (setf (gethash production-name general-productions)
                      (make-generic-production production-lhs production-rhs production-name (nreverse productions)))))
            
            (unless (nonterminal-excluded production-lhs)
              (setf (gethash production-name general-productions)
                    (create-production production-lhs production-rhs production-name)))))))
    
    
    ;Change all values of the rules hash table to contain rule structures
    ;instead of mere lists of rules.  Also check that all referenced nonterminals
    ;have been defined.
    (maphash
     #'(lambda (rule-lhs rule-productions)
         (dolist (rule-production rule-productions)
           (dolist (rhs-nonterminal (production-nonterminals rule-production))
             (unless (gethash rhs-nonterminal rules)
               (error "Nonterminal ~S used but not defined" rhs-nonterminal))))
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
            (nonterminals (coerce nonterminals-list 'simple-vector)))
        (dotimes (n (length terminals))
          (setf (gethash (svref terminals n) terminals-hash) n))
        (dotimes (n (length nonterminals))
          (setf (rule-number (gethash (svref nonterminals n) rules)) n))
        (let ((grammar (allocate-grammar
                        :argument-attributes (grammar-parametrization-argument-attributes grammar-parametrization)
                        :terminals terminals
                        :nonterminals-list nonterminals-list
                        :nonterminals nonterminals
                        :terminal-numbers terminals-hash
                        :terminal-actions (make-hash-table :test *grammar-symbol-=*)
                        :rules rules
                        :parameter-trees (make-hash-table :test *grammar-symbol-=*)
                        :max-production-length max-production-length
                        :general-productions general-productions
                        :n-productions production-number
                        :items-hash (make-hash-table :test #'equal))))
          
          ;Compute the values of derives-epsilon and initial-terminals in each rule.
          (do ((changed t))
              ((not changed))
            (setq changed nil)
            (dolist (nonterminal (grammar-nonterminals-list grammar))
              (let ((rule (grammar-rule grammar nonterminal))
                    (new-initial-terminals *empty-terminalset*)
                    (new-derives-epsilon nil))
                (dolist (production (rule-productions rule))
                  (multiple-value-bind (production-initial-terminals production-derives-epsilon)
                                       (string-initial-terminals grammar (production-rhs production))
                    (setq new-initial-terminals (terminalset-union new-initial-terminals production-initial-terminals))
                    (when production-derives-epsilon
                      (setq new-derives-epsilon t))))
                (assert-true (or new-derives-epsilon (not (rule-derives-epsilon rule))))
                (assert-true (terminalset-<= (rule-initial-terminals rule) new-initial-terminals))
                (unless (terminalset-= new-initial-terminals (rule-initial-terminals rule))
                  (setf (rule-initial-terminals rule) new-initial-terminals)
                  (setq changed t))
                (unless (eq new-derives-epsilon (rule-derives-epsilon rule))
                  (setf (rule-derives-epsilon rule) t)
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
                              (production-rhs production) *name-print-column*
                              production-number-width (print-production-number (production-number production))
                              (production-name production)))
                    (pprint-exit-if-list-exhausted)
                    (format stream "~:@_ | ")))
                (pprint-indent :block 2 stream)
                (when details
                  (format stream "~:@_Initial terminals: ~@_~@<~:[~;<epsilon> ~:_~]~{~W ~:_~}~:>"
                          (rule-derives-epsilon rule)
                          (terminalset-list grammar (rule-initial-terminals rule)))))
              (pprint-newline :mandatory stream))
            (pprint-exit-if-list-exhausted)
            (pprint-newline :mandatory stream)))
        
        ;Print the states.
        (pprint-newline :mandatory stream)
        (pprint-logical-block (stream (grammar-states grammar))
          (pprint-exit-if-list-exhausted)
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
  (depict-paragraph (markup-stream ':body-text)
    (depict markup-stream "Start nonterminal: ")
    (depict-general-nonterminal markup-stream (gramar-user-start-symbol grammar)))
  (dolist (nonterminal (grammar-nonterminals-list grammar))
    (unless (grammar-symbol-= nonterminal *start-nonterminal*)
      (depict-general-rule markup-stream (grammar-rule grammar nonterminal)))))


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