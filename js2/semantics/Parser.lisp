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

; kernel-item-alist is a list of pairs (item . prev), where item is a kernel item
; and prev is either nil or a laitem.  kernel is a list of the kernel items in a canonical order.
; Return a new state with the given list of kernel items and state number.
; For each non-null prev in kernel-item-alist, update (laitem-propagates prev) to include the
; corresponding laitem in the new state.
(defun make-state (grammar kernel kernel-item-alist number initial-lookaheads)
  (let ((laitems nil)
        (laitems-hash (make-hash-table :test #'eq)))
    (labels
      ;Create a laitem for this item and add the association item->laitem to the laitems-hash
      ;hash table if it's not there already.  Regardless of whether a new laitem was created,
      ;update the laitem's lookaheads to also include the given lookaheads.
      ;If prev is non-null, update (laitem-propagates prev) to include the laitem if it's not
      ;already included there.
      ;If a new laitem was created and its first symbol after the dot exists and is a
      ;nonterminal A, recursively close items A->.rhs corresponding to all rhs's in the
      ;grammar's rule for A.
      ((close-item (item lookaheads prev)
         (let ((laitem (gethash item laitems-hash)))
           (if laitem
             (setf (laitem-lookaheads laitem)
                   (terminalset-union (laitem-lookaheads laitem) lookaheads))
             (let ((item-next-symbol (item-next-symbol item)))
               (setq laitem (allocate-laitem grammar item lookaheads))
               (push laitem laitems)
               (setf (gethash item laitems-hash) laitem)
               (when (nonterminal? item-next-symbol)
                 (multiple-value-bind (next-lookaheads epsilon-lookahead)
                                      (string-initial-terminals grammar (rest (item-unseen item)))
                   (let ((next-prev (and epsilon-lookahead laitem)))
                     (dolist (production (rule-productions (grammar-rule grammar item-next-symbol)))
                       (close-item (make-item grammar production 0) next-lookaheads next-prev)))))))
           (when prev
             (pushnew laitem (laitem-propagates prev))))))
      
      (dolist (acons kernel-item-alist nil)
        (let ((item (car acons))
              (prev (cdr acons)))
          (close-item item initial-lookaheads prev)))
      (allocate-state number kernel (nreverse laitems)))))


; f is a function that takes two arguments:
;   a grammar symbol, and
;   a list of kernel items in order of increasing item number.
;   a list of pairs (item . prev), where item is a kernel item and prev is a laitem;
; For each possible symbol X that can be shifted while in the given state S, call
; f giving it S and the list of items that constitute the kernel of that shift's destination
; state.  The prev's are the sources of the corresponding shifted items.
(defun state-each-shift-item-alist (f state)
  (let ((shift-symbols-hash (make-hash-table :test *grammar-symbol-=*)))
    (dolist (source-laitem (state-laitems state))
      (let* ((source-item (laitem-item source-laitem))
             (shift-symbol (item-next-symbol source-item)))
        (when shift-symbol
          (push (cons (item-next source-item) source-laitem)
                (gethash shift-symbol shift-symbols-hash)))))
    ;Use dolist/gethash instead of maphash to make state assignments deterministic.
    (dolist (shift-symbol (sorted-hash-table-keys shift-symbols-hash))
      (let ((kernel-item-alist (gethash shift-symbol shift-symbols-hash)))
        (funcall f shift-symbol (sort (mapcar #'car kernel-item-alist) #'< :key #'item-number) kernel-item-alist)))))


;;; ------------------------------------------------------------------------------------------------------
;;; LR(1)


; kernel-item-alist should have the same kernel items as state.
; Return true if the prev lookaheads in kernel-item-alist are the same as or subsets of
; the corresponding lookaheads in the state's kernel laitems.
(defun state-subsumes-lookaheads (state kernel-item-alist)
  (every
   #'(lambda (acons)
       (terminalset-<= (laitem-lookaheads (cdr acons))
                       (laitem-lookaheads (state-laitem state (car acons)))))
   kernel-item-alist))


; kernel-item-alist should have the same kernel items as state.
; Return true if the prev lookaheads in kernel-item-alist are weakly compatible
; with the lookaheads in the state's kernel laitems.
(defun state-weakly-compatible (state kernel-item-alist)
  (labels
    ((lookahead-weakly-compatible (lookahead1a lookahead1b lookahead2a lookahead2b)
       (or (and (terminalsets-disjoint lookahead1a lookahead2b)
                (terminalsets-disjoint lookahead1b lookahead2a))
           (not (terminalsets-disjoint lookahead1a lookahead1b))
           (not (terminalsets-disjoint lookahead2a lookahead2b))))
     
     (lookahead-list-weakly-compatible (lookahead1a lookaheads1 lookahead2a lookaheads2)
       (or (endp lookaheads1)
           (and (lookahead-weakly-compatible lookahead1a (first lookaheads1) lookahead2a (first lookaheads2))
                (lookahead-list-weakly-compatible lookahead1a (rest lookaheads1) lookahead2a (rest lookaheads2)))))
     
     (lookahead-lists-weakly-compatible (lookaheads1 lookaheads2)
       (or (endp lookaheads1)
           (and (lookahead-list-weakly-compatible (first lookaheads1) (rest lookaheads1) (first lookaheads2) (rest lookaheads2))
                (lookahead-lists-weakly-compatible (rest lookaheads1) (rest lookaheads2))))))
    
    (or (= (length kernel-item-alist) 1)
        (lookahead-lists-weakly-compatible
         (mapcar #'(lambda (acons) (laitem-lookaheads (state-laitem state (car acons)))) kernel-item-alist)
         (mapcar #'(lambda (acons) (laitem-lookaheads (cdr acons))) kernel-item-alist)))))
  

; Propagate all lookaheads in the state.
(defun propagate-internal-lookaheads (state)
  (dolist (src-laitem (state-laitems state))
    (let ((src-lookaheads (laitem-lookaheads src-laitem)))
      (dolist (dst-laitem (laitem-propagates src-laitem))
        (setf (laitem-lookaheads dst-laitem)
              (terminalset-union (laitem-lookaheads dst-laitem) src-lookaheads))))))


; Propagate all lookaheads in kernel-item-alist, which must target destination-state.
; Mark destination-state as dirty in the dirty-states hash table.
(defun propagate-external-lookaheads (kernel-item-alist destination-state dirty-states)
  (dolist (acons kernel-item-alist)
    (let ((dest-laitem (state-laitem destination-state (car acons)))
          (src-laitem (cdr acons)))
      (setf (laitem-lookaheads dest-laitem)
            (terminalset-union (laitem-lookaheads dest-laitem) (laitem-lookaheads src-laitem)))))
  (setf (gethash destination-state dirty-states) t))


; Make all states in the grammar and return the initial state.
; Initialize the grammar's list of states.
; Set up the laitems' propagate lists but do not propagate lookaheads yet.
; Initialize the states' gotos lists.
; Initialize the states' shift (but not reduce or accept) transitions in the transitions lists.
(defun add-all-lr-states (grammar)
  (let* ((initial-item (make-item grammar (grammar-start-production grammar) 0))
         (lr-states-hash (make-hash-table :test #'equal))  ;kernel -> list of states with that kernel
         (initial-kernel (list initial-item))
         (initial-state (make-state grammar initial-kernel (list (cons initial-item nil)) 0 (make-terminalset grammar *end-marker*)))
         (states (list initial-state))
         (next-state-number 1))
    (setf (gethash initial-kernel lr-states-hash) (list initial-state))
    (do ((source-states (list initial-state))
         (dirty-states (make-hash-table :test #'eq)))      ;Set of states whose kernel lookaheads changed and haven't been propagated yet
        ((and (endp source-states) (zerop (hash-table-count dirty-states))))
      (labels
        ((make-destination-state (kernel kernel-item-alist)
           (let* ((possible-destination-states (gethash kernel lr-states-hash))
                  (destination-state (find-if #'(lambda (possible-destination-state)
                                                  (state-subsumes-lookaheads possible-destination-state kernel-item-alist))
                                              possible-destination-states)))
             (cond
              (destination-state)
              ((setq destination-state (find-if #'(lambda (possible-destination-state)
                                                    (state-weakly-compatible possible-destination-state kernel-item-alist))
                                                possible-destination-states))
               (propagate-external-lookaheads kernel-item-alist destination-state dirty-states))
              (t
               (setq destination-state (make-state grammar kernel kernel-item-alist next-state-number *empty-terminalset*))
               (propagate-external-lookaheads kernel-item-alist destination-state dirty-states)
               (push destination-state (gethash kernel lr-states-hash))
               (incf next-state-number)
               (push destination-state states)
               (push destination-state source-states)))
             destination-state))
         
         (update-destination-state (destination-state kernel-item-alist)
           (cond
            ((state-subsumes-lookaheads destination-state kernel-item-alist)
             destination-state)
            ((state-weakly-compatible destination-state kernel-item-alist)
             (propagate-external-lookaheads kernel-item-alist destination-state dirty-states)
             destination-state)
            (t (make-destination-state (state-kernel destination-state) kernel-item-alist)))))
        
        (if source-states
          (let ((source-state (pop source-states)))
            (remhash source-state dirty-states)
            (propagate-internal-lookaheads source-state)
            (state-each-shift-item-alist
             #'(lambda (shift-symbol kernel kernel-item-alist)
                 (let ((destination-state (make-destination-state kernel kernel-item-alist)))
                   (if (nonterminal? shift-symbol)
                     (push (cons shift-symbol destination-state)
                           (state-gotos source-state))
                     (push (cons shift-symbol (make-shift-transition destination-state))
                           (state-transitions source-state)))))
             source-state))
          (dolist (dirty-state (sort (hash-table-keys dirty-states) #'< :key #'state-number))
            (when (remhash dirty-state dirty-states)
              (propagate-internal-lookaheads dirty-state)
              (state-each-shift-item-alist
               #'(lambda (shift-symbol kernel kernel-item-alist)
                   (declare (ignore kernel))
                   (if (nonterminal? shift-symbol)
                     (let* ((destination-binding (assoc shift-symbol (state-gotos dirty-state) :test *grammar-symbol-=*))
                            (destination-state (assert-non-null (cdr destination-binding))))
                       (setf (cdr destination-binding) (update-destination-state destination-state kernel-item-alist)))
                     (let* ((destination-transition (cdr (assoc shift-symbol (state-transitions dirty-state) :test *grammar-symbol-=*)))
                            (destination-state (assert-non-null (transition-state destination-transition))))
                       (setf (transition-state destination-transition)
                             (update-destination-state destination-state kernel-item-alist)))))
               dirty-state))))))
    (setf (grammar-states grammar) (nreverse states))
    initial-state))


;;; ------------------------------------------------------------------------------------------------------
;;; LALR(1)


; Make all states in the grammar and return the initial state.
; Initialize the grammar's list of states.
; Set up the laitems' propagate lists but do not propagate lookaheads yet.
; Initialize the states' gotos lists.
; Initialize the states' shift (but not reduce or accept) transitions in the transitions lists.
(defun add-all-lalr-states (grammar)
  (let* ((initial-item (make-item grammar (grammar-start-production grammar) 0))
         (lalr-states-hash (make-hash-table :test #'equal))  ;kernel -> state
         (initial-kernel (list initial-item))
         (initial-state (make-state grammar initial-kernel (list (cons initial-item nil)) 0 (make-terminalset grammar *end-marker*)))
         (states (list initial-state))
         (next-state-number 1))
    (setf (gethash initial-kernel lalr-states-hash) initial-state)
    (do ((source-states (list initial-state)))
        ((endp source-states))
      (let ((source-state (pop source-states)))
        (state-each-shift-item-alist
         #'(lambda (shift-symbol kernel kernel-item-alist)
             (let ((destination-state (gethash kernel lalr-states-hash)))
               (if destination-state
                 (dolist (acons kernel-item-alist)
                   (pushnew (state-laitem destination-state (car acons)) (laitem-propagates (cdr acons))))
                 (progn
                   (setq destination-state (make-state grammar kernel kernel-item-alist next-state-number *empty-terminalset*))
                   (setf (gethash kernel lalr-states-hash) destination-state)
                   (incf next-state-number)
                   (push destination-state states)
                   (push destination-state source-states)))
               (if (nonterminal? shift-symbol)
                 (push (cons shift-symbol destination-state)
                       (state-gotos source-state))
                 (push (cons shift-symbol (make-shift-transition destination-state))
                       (state-transitions source-state)))))
         source-state)))
    (setf (grammar-states grammar) (nreverse states))
    initial-state))


; Propagate the lookaheads in the LALR(1) grammar.
(defun propagate-lalr-lookaheads (grammar)
  (let ((dirty-laitems (make-hash-table :test #'eq)))
    (dolist (state (grammar-states grammar))
      (dolist (laitem (state-laitems state))
        (when (and (laitem-propagates laitem) (not (terminalset-empty? (laitem-lookaheads laitem))))
          (setf (gethash laitem dirty-laitems) t))))
    (do ()
        ((zerop (hash-table-count dirty-laitems)))
      (dolist (dirty-laitem (hash-table-keys dirty-laitems))
        (remhash dirty-laitem dirty-laitems)
        (let ((src-lookaheads (laitem-lookaheads dirty-laitem)))
          (dolist (dst-laitem (laitem-propagates dirty-laitem))
            (let* ((old-dst-lookaheads (laitem-lookaheads dst-laitem))
                   (new-dst-lookaheads (terminalset-union old-dst-lookaheads src-lookaheads)))
              (unless (terminalset-= old-dst-lookaheads new-dst-lookaheads)
                (setf (laitem-lookaheads dst-laitem) new-dst-lookaheads)
                (setf (gethash dst-laitem dirty-laitems) t)))))))
    
    ;Erase the propagates chains in all laitems.
    (dolist (state (grammar-states grammar))
      (dolist (laitem (state-laitems state))
        (setf (laitem-propagates laitem) nil)))))


;;; ------------------------------------------------------------------------------------------------------


; Calculate the reduce and accept transitions in the grammar.
; Also sort all transitions by their terminal numbers and gotos by their nonterminal numbers.
; Conflicting transitions are sorted as follows:
;    shifts come before reduces and accepts
;    accepts come before reduces
;    reduces with lower production numbers come before reduces with higher production numbers
; Disambiguation will choose the first member of a sorted list of conflicting transitions.
(defun finish-transitions (grammar)
  (dolist (state (grammar-states grammar))
    (dolist (laitem (state-laitems state))
      (let ((item (laitem-item laitem)))
        (unless (item-next-symbol item)
          (if (grammar-symbol-= (item-lhs item) *start-nonterminal*)
            (when (terminal-in-terminalset grammar *end-marker* (laitem-lookaheads laitem))
              (push (cons *end-marker* (make-accept-transition))
                    (state-transitions state)))
            (map-terminalset-reverse
             #'(lambda (lookahead)
                 (push (cons lookahead (make-reduce-transition (item-production item)))
                       (state-transitions state)))
             grammar
             (laitem-lookaheads laitem))))))
    (setf (state-gotos state)
          (sort (state-gotos state) #'< :key #'(lambda (goto-cons) (state-number (cdr goto-cons)))))
    (setf (state-transitions state)
          (sort (state-transitions state)
                #'(lambda (transition-cons-1 transition-cons-2)
                    (let ((terminal-number-1 (terminal-number grammar (car transition-cons-1)))
                          (terminal-number-2 (terminal-number grammar (car transition-cons-2))))
                      (cond
                       ((< terminal-number-1 terminal-number-2) t)
                       ((> terminal-number-1 terminal-number-2) nil)
                       (t (let* ((transition1 (cdr transition-cons-1))
                                 (transition2 (cdr transition-cons-2))
                                 (transition-kind-1 (transition-kind transition1))
                                 (transition-kind-2 (transition-kind transition2)))
                            (cond
                             ((eq transition-kind-2 :shift) nil)
                             ((eq transition-kind-1 :shift) t)
                             ((eq transition-kind-2 :accept) nil)
                             ((eq transition-kind-1 :accept) t)
                             (t (let ((production-number-1 (production-number (transition-production transition1)))
                                      (production-number-2 (production-number (transition-production transition2))))
                                  (< production-number-1 production-number-2)))))))))))))


; Find ambiguities, if any, in the grammar.  Report them on the given stream.
; Fix all ambiguities in favor of the first transition listed
; (the transitions were ordered by finish-transitions).
(defun report-and-fix-ambiguities (grammar stream)
  (let ((found-ambiguities nil))
    (pprint-logical-block (stream nil)
      (dolist (state (grammar-states grammar))
        (labels
          
          ((report-ambiguity (transition-cons other-transition-conses)
             (unless found-ambiguities
               (setq found-ambiguities t)
               (format stream "~&Ambiguities:")
               (pprint-indent :block 2 stream))
             (pprint-newline :mandatory stream)
             (pprint-logical-block (stream nil)
               (format stream "S~D: ~W ~:_=> ~:_" (state-number state) (car transition-cons))
               (pprint-logical-block (stream nil)
                 (dolist (a (cons transition-cons other-transition-conses))
                   (print-transition (cdr a) stream)
                   (format stream "   ~:_")))))
           
           ; Check the list of transition-conses and report ambiguities.
           ; start is the start of a possibly larger list of transition-conses whose tail
           ; is the given list.  If ambiguities exist, return a copy of start up to the
           ; position of list in it followed by list with ambiguities removed.  If not,
           ; return start unchanged.
           (check (transition-conses start)
             (if transition-conses
               (let* ((transition-cons (first transition-conses))
                      (transition-terminal (car transition-cons))
                      (transition-conses-rest (rest transition-conses)))
                 (if transition-conses-rest
                   (if (grammar-symbol-= transition-terminal (car (first transition-conses-rest)))
                     (let ((unrelated-transitions
                            (member-if #'(lambda (a) (not (grammar-symbol-= transition-terminal (car a))))
                                       transition-conses-rest)))
                       (report-ambiguity transition-cons (ldiff transition-conses-rest unrelated-transitions))
                       (check unrelated-transitions (append (ldiff start transition-conses-rest) unrelated-transitions)))
                     (check transition-conses-rest start))
                   start))
               start)))
          
          (let ((transition-conses (state-transitions state)))
            (setf (state-transitions state) (check transition-conses transition-conses))))))
    (when found-ambiguities
      (pprint-newline :mandatory stream))))


; Erase the existing parser, if any, for the given grammar.
(defun clear-parser (grammar)
  (clrhash (grammar-items-hash grammar))
  (setf (grammar-states grammar) nil))


; Construct a LR or LALR parser in the given grammar.  kind should be either :lalr-1 or :lr-1.
; Return the grammar.
(defun compile-parser (grammar kind)
  (clear-parser grammar)
  (ecase kind
    (:lalr-1
     (add-all-lalr-states grammar)
     (propagate-lalr-lookaheads grammar))
    (:lr-1
     (add-all-lr-states grammar)))
  (finish-transitions grammar)
  (report-and-fix-ambiguities grammar *error-output*)
  grammar)


; Make the grammar and compile its parser.  kind should be either :lalr-1 or :lr-1.
(defun make-and-compile-grammar (kind parametrization start-symbol grammar-source &optional excluded-nonterminals-source)
  (compile-parser (make-grammar parametrization start-symbol grammar-source excluded-nonterminals-source)
                  kind))


;;; ------------------------------------------------------------------------------------------------------

; Parse the input list of tokens to produce a parse tree.
; token-terminal is a function that returns a terminal symbol when given an input token.
(defun parse (grammar token-terminal input)
  (labels
    (;Continue the parse with the given parser stack and remainder of input.
     (parse-step (stack input)
       (if (endp input)
         (parse-step-1 stack *end-marker* nil nil)
         (let ((token (first input)))
           (parse-step-1 stack (funcall token-terminal token) token (rest input)))))
     
     ;Same as parse-step except that the next input terminal has been determined already.
     ;input-rest contains the input tokens after the next token.
     (parse-step-1 (stack terminal token input-rest)
       (let* ((state (caar stack))
              (transition (cdr (assoc terminal (state-transitions state) :test *grammar-symbol-=*))))
         (if transition
           (case (transition-kind transition)
             (:shift (parse-step (acons (transition-state transition) token stack) input-rest))
             (:reduce (let ((production (transition-production transition))
                            (expansion nil))
                        (dotimes (i (production-rhs-length production))
                          (push (cdr (pop stack)) expansion))
                        (let* ((state (caar stack))
                               (dst-state (assert-non-null
                                           (cdr (assoc (production-lhs production) (state-gotos state) :test *grammar-symbol-=*))))
                               (named-expansion (cons (production-name production) expansion)))
                          (parse-step-1 (acons dst-state named-expansion stack) terminal token input-rest))))
             (:accept (cdar stack))
             (t (error "Bad transition: ~S" transition)))
           (error "Parse error on ~S followed by ~S ..." token (ldiff input-rest (nthcdr 10 input-rest)))))))
    
    (parse-step (list (cons (grammar-start-state grammar) nil)) input)))


;;; ------------------------------------------------------------------------------------------------------
;;; ACTIONS

; Initialize the action-signatures hash table, setting each grammar symbol's signature
; to null for now.  Also clear all production actions in the grammar.
(defun clear-actions (grammar)
  (let ((action-signatures (make-hash-table :test *grammar-symbol-=*))
        (terminals (grammar-terminals grammar))
        (nonterminals (grammar-nonterminals grammar)))
    (dotimes (i (length terminals))
      (setf (gethash (svref terminals i) action-signatures) nil))
    (dotimes (i (length nonterminals))
      (setf (gethash (svref nonterminals i) action-signatures) nil))
    (setf (grammar-action-signatures grammar) action-signatures)
    (each-grammar-production
     grammar
     #'(lambda (production)
         (setf (production-actions production) nil)
         (setf (production-n-action-args production) nil)
         (setf (production-evaluator-code production) nil)
         (setf (production-evaluator production) nil)))
    (clrhash (grammar-terminal-actions grammar))))


; Declare the type of action action-symbol, when called on general-grammar-symbol, to be type-expr.
; Signal an error on duplicate actions.
; It's OK if some of the symbol instances don't exist, as long as at least one does.
(defun declare-action (grammar general-grammar-symbol action-symbol type-expr)
  (unless (and action-symbol (symbolp action-symbol))
    (error "Bad action name ~S" action-symbol))
  (let ((action-signatures (grammar-action-signatures grammar))
        (grammar-symbols (general-grammar-symbol-instances grammar general-grammar-symbol))
        (symbol-exists nil))
    (dolist (grammar-symbol grammar-symbols)
      (let ((signature (gethash grammar-symbol action-signatures :undefined)))
        (unless (eq signature :undefined)
          (setq symbol-exists t)
          (when (assoc action-symbol signature :test #'eq)
            (error "Attempt to redefine the type of action ~S on ~S" action-symbol grammar-symbol))
          (setf (gethash grammar-symbol action-signatures)
                (nconc signature (list (cons action-symbol type-expr))))
          (if (nonterminal? grammar-symbol)
            (dolist (production (rule-productions (grammar-rule grammar grammar-symbol)))
              (setf (production-actions production)
                    (nconc (production-actions production) (list (cons action-symbol nil)))))
            (let ((terminal-actions (grammar-terminal-actions grammar)))
              (assert-type grammar-symbol terminal)
              (setf (gethash grammar-symbol terminal-actions)
                    (nconc (gethash grammar-symbol terminal-actions) (list (cons action-symbol nil)))))))))
    (unless symbol-exists
      (error "Bad action grammar symbol ~S" grammar-symbols))))


; Return the list of pairs (action-symbol . type-or-type-expr) for this grammar-symbol.
; The pairs are in order from oldest to newest action-symbols added to this grammar-symbol.
(declaim (inline grammar-symbol-signature))
(defun grammar-symbol-signature (grammar grammar-symbol)
  (gethash grammar-symbol (grammar-action-signatures grammar)))


; Return the list of action types of the grammar's user start-symbol.
(defun grammar-user-start-action-types (grammar)
  (mapcar #'cdr (grammar-symbol-signature grammar (gramar-user-start-symbol grammar))))


; If action action-symbol is declared on grammar-symbol, return two values:
;   t, and
;   the action's type-expr;
; If not, return nil.
(defun action-declaration (grammar grammar-symbol action-symbol)
  (let ((declaration (assoc action-symbol (grammar-symbol-signature grammar grammar-symbol) :test #'eq)))
    (and declaration
         (values t (cdr declaration)))))


; Call f on every action declaration, passing it two arguments:
;   the grammar-symbol;
;   a pair (action-symbol . type-expr).
; f may modify the action's type-expr.
(defun each-action-declaration (grammar f)
  (maphash #'(lambda (grammar-symbol signature)
               (dolist (action-declaration signature)
                 (funcall f grammar-symbol action-declaration)))
           (grammar-action-signatures grammar)))


; Define action action-symbol, when called on the production with the given name,
; to be action-expr.  The action should have been declared already.
(defun define-action (grammar production-name action-symbol action-expr)
  (dolist (production (general-production-productions (grammar-general-production grammar production-name)))
    (let ((definition (assoc action-symbol (production-actions production) :test #'eq)))
      (cond
       ((null definition)
        (error "Attempt to define action ~S on ~S, which hasn't been declared yet" action-symbol production-name))
       ((cdr definition)
        (error "Duplicate definition of action ~S on ~S" action-symbol production-name))
       (t (setf (cdr definition) (make-action action-expr)))))))


; Define action action-symbol, when called on the given terminal,
; to execute the given function, which should take a token as an input and
; produce a value of the proper type as output.
; The action should have been declared already.
(defun define-terminal-action (grammar terminal action-symbol action-function)
  (assert-type action-function function)
  (let ((definition (assoc action-symbol (gethash terminal (grammar-terminal-actions grammar)) :test #'eq)))
    (cond
     ((null definition)
      (error "Attempt to define action ~S on ~S, which hasn't been declared yet" action-symbol terminal))
     ((cdr definition)
      (error "Duplicate definition of action ~S on ~S" action-symbol terminal))
     (t (setf (cdr definition) action-function)))))



; Parse the input list of tokens to produce a list of action results.
; token-terminal is a function that returns a terminal symbol when given an input token.
; If trace is:
;   nil,     don't print trace information
;   :code,   print trace information, including action code
;   other    print trace information
; Return two values:
;   the list of action results;
;   the list of action results' types.
(defun action-parse (grammar token-terminal input &key trace)
  (labels
    (;Continue the parse with the given stacks and remainder of input.
     (parse-step (state-stack value-stack input)
       (if (endp input)
         (parse-step-1 state-stack value-stack *end-marker* nil nil)
         (let ((token (first input)))
           (parse-step-1 state-stack value-stack (funcall token-terminal token) token (rest input)))))
     
     ;Same as parse-step except that the next input terminal has been determined already.
     ;input-rest contains the input tokens after the next token.
     (parse-step-1 (state-stack value-stack terminal token input-rest)
       (let* ((state (car state-stack))
              (transition (cdr (assoc terminal (state-transitions state) :test *grammar-symbol-=*))))
         (if transition
           (case (transition-kind transition)
             (:shift
              (dolist (action-function-binding (gethash terminal (grammar-terminal-actions grammar)))
                (push (funcall (cdr action-function-binding) token) value-stack))
              (parse-step (cons (transition-state transition) state-stack) value-stack input-rest))
             (:reduce
              (let* ((production (transition-production transition))
                     (state-stack (nthcdr (production-rhs-length production) state-stack))
                     (state (car state-stack))
                     (dst-state (assert-non-null
                                 (cdr (assoc (production-lhs production) (state-gotos state) :test *grammar-symbol-=*))))
                     (value-stack (funcall (production-evaluator production) value-stack)))
                (parse-step-1 (cons dst-state state-stack) value-stack terminal token input-rest)))
             (:accept (values (nreverse value-stack) (grammar-user-start-action-types grammar)))
             (t (error "Bad transition: ~S" transition)))
           (error "Parse error on ~S followed by ~S ..." token (ldiff input-rest (nthcdr 10 input-rest)))))))
    
    (if trace
      (trace-action-parse grammar token-terminal input trace)
      (parse-step (list (grammar-start-state grammar)) nil input))))


; Same as action-parse, but with tracing information
; If trace is:
;   :code,   print trace information, including action code
;   other    print trace information
; Return two values:
;   the list of action results;
;   the list of action results' types.
(defun trace-action-parse (grammar token-terminal input trace)
  (labels
    (;Continue the parse with the given stacks and remainder of input.
     ;type-stack contains the types of corresponding value-stack entries.
     (parse-step (state-stack value-stack type-stack input)
       (if (endp input)
         (parse-step-1 state-stack value-stack type-stack *end-marker* nil nil)
         (let ((token (first input)))
           (parse-step-1 state-stack value-stack type-stack (funcall token-terminal token) token (rest input)))))
     
     ;Same as parse-step except that the next input terminal has been determined already.
     ;input-rest contains the input tokens after the next token.
     (parse-step-1 (state-stack value-stack type-stack terminal token input-rest)
       (let* ((state (car state-stack))
              (transition (cdr (assoc terminal (state-transitions state) :test *grammar-symbol-=*))))
         (format *trace-output* "S~D: ~@_" (state-number state))
         (print-values (reverse value-stack) (reverse type-stack) *trace-output*)
         (pprint-newline :mandatory *trace-output*)
         (if transition
           (case (transition-kind transition)
             (:shift
              (format *trace-output* "  shift ~W~:@_" terminal)
              (dolist (action-function-binding (gethash terminal (grammar-terminal-actions grammar)))
                (push (funcall (cdr action-function-binding) token) value-stack))
              (dolist (action-signature (grammar-symbol-signature grammar terminal))
                (push (cdr action-signature) type-stack))
              (parse-step (cons (transition-state transition) state-stack) value-stack type-stack input-rest))
             (:reduce
              (let ((production (transition-production transition)))
                (write-string "  reduce " *trace-output*)
                (if (eq trace :code)
                  (write production :stream *trace-output* :pretty t)
                  (print-production production *trace-output*))
                (pprint-newline :mandatory *trace-output*)
                (let* ((state-stack (nthcdr (production-rhs-length production) state-stack))
                       (state (car state-stack))
                       (dst-state (assert-non-null
                                   (cdr (assoc (production-lhs production) (state-gotos state) :test *grammar-symbol-=*))))
                       (value-stack (funcall (production-evaluator production) value-stack))
                       (type-stack (nthcdr (production-n-action-args production) type-stack)))
                  (dolist (action-signature (grammar-symbol-signature grammar (production-lhs production)))
                    (push (cdr action-signature) type-stack))
                  (parse-step-1 (cons dst-state state-stack) value-stack type-stack terminal token input-rest))))
             (:accept
              (format *trace-output* "  accept~:@_")
              (values (nreverse value-stack) (nreverse type-stack)))
             (t (error "Bad transition: ~S" transition)))
           (error "Parse error on ~S followed by ~S ..." token (ldiff input-rest (nthcdr 10 input-rest)))))))
    
    (parse-step (list (grammar-start-state grammar)) nil nil input)))

