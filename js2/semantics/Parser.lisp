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

; kernel-item-alist is a list of pairs (item . prev), where item is a kernel item
; and prev is either nil or a laitem.  kernel is a list of the kernel items in a canonical order.
; Return a new state with the given list of kernel items and state number.
; If mode is :lalr-1, for each non-null prev in kernel-item-alist, update
; (laitem-propagates prev) to include the corresponding laitem in the new state.  Do this anyway
; for internal lookaheads, regardless of mode.
;
; If mode is :canonical-lr-1, kernel-item-alist is a list of pairs (item . lookaheads), where
; lookaheads is a terminalset of lookaheads for that item.  Use these lookaheads instead of
; initial-lookaheads.
(defun make-state (grammar kernel kernel-item-alist mode number initial-lookaheads)
  (let ((laitems nil)
        (laitems-hash (make-hash-table :test #'eq))
        (laitems-maybe-forbidden nil)) ;Association list of:  laitem -> terminalset of potentially forbidden terminals; missing means *empty-terminalset*
    (labels
      ;Create a laitem for this item and add the association item->laitem to the laitems-hash
      ;hash table if it's not there already.  Regardless of whether a new laitem was created,
      ;update the laitem's lookaheads to also include the given lookaheads.
      ;forbidden is a terminalset of terminals that must not occur immediately after the dot in this
      ;laitem.  The forbidden set is inherited from constraints in parent laitems in the same state.
      ;maybe-forbidden is an upper bounds on the forbidden lookaheads in this laitem.
      ;If prev is non-null, update (laitem-propagates prev) to include the laitem and the given
      ;passthrough terminalset if it's not already included there.
      ;If a new laitem was created and its first symbol after the dot exists and is a
      ;nonterminal A, recursively close items A->.rhs corresponding to all rhs's in the
      ;grammar's rule for A.
      ((close-item (item forbidden maybe-forbidden lookaheads prev passthroughs)
         (let ((production (item-production item))
               (dot (item-dot item))
               (laitem (gethash item laitems-hash)))
           (let ((extra-forbidden (terminalset-complement (general-production-constraint production dot))))
             (terminalset-union-f forbidden extra-forbidden)
             (terminalset-union-f maybe-forbidden extra-forbidden))
           (unless (terminalset-empty? forbidden)
             (multiple-value-bind (dot-lookaheads dot-passthroughs)
                                  (string-initial-terminals grammar (item-unseen item) (production-constraints production) (item-dot item) t)
               (let ((dot-initial (terminalset-union dot-lookaheads dot-passthroughs)))
                 ;Check whether any terminal can start this item.  If not, skip this item altogether.
                 (when (terminalset-empty? (terminalset-difference dot-initial forbidden))
                   ;Mark skipped items in the laitems-hash table.
                   (when (and laitem (not (eq laitem 'forbidden)))
                     (error "Two laitems in the same state differing only in forbidden initial terminal constraints: ~S" laitem))
                   (setf (gethash item laitems-hash) 'forbidden)
                   (return-from close-item))
                 ;Convert forbidden into a canonical format by removing terminals that cannot begin this item's expansion anyway.
                 (terminalset-intersection-f forbidden dot-initial))))
           (if laitem
             (let ((laitem-maybe-forbidden-entry (assoc laitem laitems-maybe-forbidden))
                   (new-forbidden (terminalset-union forbidden (laitem-forbidden laitem))))
               (when laitem-maybe-forbidden-entry
                 (terminalset-intersection-f (cdr laitem-maybe-forbidden-entry) maybe-forbidden))
               (unless (terminalset-<= new-forbidden (or (cdr laitem-maybe-forbidden-entry) *empty-terminalset*))
                 (error "Two laitems in the same state differing only in forbidden initial terminal constraints: ~S ~%old forbidden: ~S ~%new forbidden: ~S~%maybe forbidden: ~S"
                        laitem
                        (terminalset-list grammar (laitem-forbidden laitem))
                        (terminalset-list grammar forbidden)
                        (and laitem-maybe-forbidden-entry (terminalset-list grammar (cdr laitem-maybe-forbidden-entry)))))
               (setf (laitem-forbidden laitem) new-forbidden)
               (terminalset-union-f (laitem-lookaheads laitem) lookaheads))
             (let ((item-next-symbol (item-next-symbol item)))
               (setq laitem (allocate-laitem grammar item forbidden lookaheads))
               (push laitem laitems)
               (setf (gethash item laitems-hash) laitem)
               (unless (terminalset-empty? maybe-forbidden)
                 (push (cons laitem maybe-forbidden) laitems-maybe-forbidden))
               (when (nonterminal? item-next-symbol)
                 (multiple-value-bind (next-lookaheads next-passthroughs)
                                      (string-initial-terminals grammar (rest (item-unseen item)) (production-constraints production) (1+ dot) nil)
                   (let ((next-prev (and (not (terminalset-empty? next-passthroughs)) laitem)))
                     (dolist (production (rule-productions (grammar-rule grammar item-next-symbol)))
                       (close-item (make-item grammar production 0) forbidden maybe-forbidden next-lookaheads next-prev next-passthroughs)))))))
           (when prev
             (laitem-add-propagation prev laitem passthroughs)))))
      
      (dolist (acons kernel-item-alist)
        (close-item (car acons) 
                    *empty-terminalset*
                    *empty-terminalset*
                    (if (eq mode :canonical-lr-1) (cdr acons) initial-lookaheads)
                    (and (eq mode :lalr-1) (cdr acons))
                    *full-terminalset*))
      (allocate-state number kernel (nreverse laitems)))))


; f is a function that takes three arguments:
;   a grammar symbol;
;   a list of kernel items in order of increasing item number [list of (item . lookahead) when mode is :canonical-lr-1];
;   a list of pairs (item . prev), where item is a kernel item and prev is a laitem.
; For each possible symbol X that can be shifted while in the given state S, call
; f giving it S and the list of items that constitute the kernel of that shift's destination
; state.  The prev's are the sources of the corresponding shifted items.
(defun state-each-shift-item-alist (f state mode)
  (let ((shift-symbols-hash (make-hash-table :test *grammar-symbol-=*)))
    (dolist (source-laitem (state-laitems state))
      (let* ((source-item (laitem-item source-laitem))
             (shift-symbol (item-next-symbol source-item)))
        (when shift-symbol
          (push (cons (item-next source-item) source-laitem)
                (gethash shift-symbol shift-symbols-hash)))))
    ;Use dolist/gethash instead of maphash to make state assignments deterministic.
    (dolist (shift-symbol (sorted-hash-table-keys shift-symbols-hash))
      (let* ((kernel-item-alist (gethash shift-symbol shift-symbols-hash))
             (kernel (if (eq mode :canonical-lr-1)
                       (sort (mapcar #'(lambda (acons)
                                         (cons (car acons) (laitem-lookaheads (cdr acons))))
                                     kernel-item-alist)
                             #'<
                             :key #'(lambda (acons) (item-number (car acons))))
                       (sort (mapcar #'car kernel-item-alist) #'< :key #'item-number))))
        (funcall f shift-symbol kernel kernel-item-alist)))))


; f is a function that takes a terminal variant as an argument.
; For each variant of the given terminal (which, along with kernel-item-alist, was obtained from
; state-each-shift-item-alist's callback), determine whether that variant can actually occur at the
; current position or whether it is forbidden by constraints.  If it can occur, call f with that variant.
; Signal an error if some laitems in kernel-item-alist indicate that a variant can occur while others
; indicate that the same variant cannot occur.  Also signal an internal error if no variant can occur, as
; make-state should have filtered such shift items out.
(defun each-shift-symbol-variant (f grammar terminal kernel-item-alist)
  (let ((n-applicable-variants 0))
    (dolist (variant (terminal-variants grammar terminal))
      (let ((allowed nil)
            (forbidden nil))
        (dolist (acons kernel-item-alist)
          (if (terminal-in-terminalset grammar variant (laitem-forbidden (cdr acons)))
            (setq forbidden t)
            (setq allowed t)))
        (when (eq allowed forbidden)
          (error "Symbol ~S ~A" variant
                 (if allowed "both allowed and forbidden" "neither allowed nor forbidden")))
        (unless forbidden
          (incf n-applicable-variants)
          (funcall f variant))))
    (when (zerop n-applicable-variants)
      (error "Internal parser error"))))


;;; ------------------------------------------------------------------------------------------------------
;;; CANONICAL LR(1)
;;;
;;; Canonical LR(1) is accepts the same set of languages as LR(1) except that it produces vastly larger,
;;; unoptimizied state tables.  The only advantage to using Canonical LR(1) instead of LR(1) is that
;;; a Canonical LR(1) parser will not make any reductions from an error state, whereas a LR(1) or LALR(1)
;;; parser might make reductions (but not shifts).  In other words, a Canonical LR(1) parser's shift and
;;; reduce tables are fully accurate rather than conservative approximations based on merged states.


; Make all states in the grammar and return the initial state.
; Initialize the grammar's list of states.
; Initialize the states' gotos lists.
; Initialize the states' shift (but not reduce or accept) transitions in the transitions lists.
(defun add-all-canonical-lr-states (grammar)
  (let* ((initial-item (make-item grammar (grammar-start-production grammar) 0))
         (lr-states-hash (make-hash-table :test #'equal))  ;canonical kernel -> state
         (initial-kernel (list (cons initial-item (make-terminalset grammar *end-marker*))))
         (initial-state (make-state grammar initial-kernel initial-kernel :canonical-lr-1 0 nil))
         (states (list initial-state))
         (next-state-number 1))
    (setf (gethash initial-kernel lr-states-hash) initial-state)
    (do ((source-states (list initial-state)))
        ((endp source-states))
      (let ((source-state (pop source-states)))
        ;Propagate the source state's internal lookaheads and then erase the propagates chains.
        (propagate-internal-lookaheads source-state)
        (dolist (laitem (state-laitems source-state))
          (setf (laitem-propagates laitem) nil))
        
        (state-each-shift-item-alist
         #'(lambda (shift-symbol kernel kernel-item-alist)
             (let ((destination-state (gethash kernel lr-states-hash)))
               (unless destination-state
                 (setq destination-state (make-state grammar kernel kernel :canonical-lr-1 next-state-number nil))
                 (setf (gethash kernel lr-states-hash) destination-state)
                 (incf next-state-number)
                 (push destination-state states)
                 (push destination-state source-states))
               (if (nonterminal? shift-symbol)
                 (push (cons shift-symbol destination-state)
                       (state-gotos source-state))
                 (each-shift-symbol-variant
                  #'(lambda (shift-symbol-variant)
                      (push (cons shift-symbol-variant (make-shift-transition destination-state))
                            (state-transitions source-state)))
                  grammar shift-symbol kernel-item-alist))))
         source-state :canonical-lr-1)))
    (setf (grammar-states grammar) (nreverse states))
    initial-state))


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
  (do ((changed t))
      ((not changed))
    (setq changed nil)
    (dolist (src-laitem (state-laitems state))
      (let ((src-lookaheads (laitem-lookaheads src-laitem)))
        (dolist (propagation (laitem-propagates src-laitem))
          (let* ((dst-laitem (car propagation))
                 (mask (cdr propagation))
                 (old-dst-lookaheads (laitem-lookaheads dst-laitem))
                 (new-dst-lookaheads (terminalset-union old-dst-lookaheads (terminalset-intersection src-lookaheads mask))))
            (setf (laitem-lookaheads dst-laitem) new-dst-lookaheads)
            (unless (terminalset-= old-dst-lookaheads new-dst-lookaheads)
              (setq changed t))))))))


; Propagate all lookaheads in kernel-item-alist, which must target destination-state.
; Mark destination-state as dirty in the dirty-states hash table.
(defun propagate-external-lookaheads (kernel-item-alist destination-state dirty-states)
  (dolist (acons kernel-item-alist)
    (let ((dest-laitem (state-laitem destination-state (car acons)))
          (src-laitem (cdr acons)))
      (terminalset-union-f (laitem-lookaheads dest-laitem) (laitem-lookaheads src-laitem))))
  (setf (gethash destination-state dirty-states) t))


; Make all states in the grammar and return the initial state.
; Initialize the grammar's list of states.
; Initialize the states' gotos lists.
; Initialize the states' shift (but not reduce or accept) transitions in the transitions lists.
(defun add-all-lr-states (grammar)
  (let* ((initial-item (make-item grammar (grammar-start-production grammar) 0))
         (lr-states-hash (make-hash-table :test #'equal))  ;kernel -> list of states with that kernel
         (initial-kernel (list initial-item))
         (initial-state (make-state grammar initial-kernel (list (cons initial-item nil)) :lr-1 0 (make-terminalset grammar *end-marker*)))
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
               (setq destination-state (make-state grammar kernel kernel-item-alist :lr-1 next-state-number *empty-terminalset*))
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
                     (each-shift-symbol-variant
                      #'(lambda (shift-symbol-variant)
                          (push (cons shift-symbol-variant (make-shift-transition destination-state))
                                (state-transitions source-state)))
                      grammar shift-symbol kernel-item-alist))))
             source-state :lr-1))
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
                     (each-shift-symbol-variant
                      #'(lambda (shift-symbol-variant)
                          (let* ((destination-transition (state-transition dirty-state shift-symbol-variant))
                                 (destination-state (assert-non-null (transition-state destination-transition))))
                            (setf (transition-state destination-transition)
                                  (update-destination-state destination-state kernel-item-alist))))
                      grammar shift-symbol kernel-item-alist)))
               dirty-state :lr-1))))))
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
         (initial-state (make-state grammar initial-kernel (list (cons initial-item nil)) :lalr-1 0 (make-terminalset grammar *end-marker*)))
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
                   (laitem-add-propagation (cdr acons) (state-laitem destination-state (car acons)) *full-terminalset*))
                 (progn
                   (setq destination-state (make-state grammar kernel kernel-item-alist :lalr-1 next-state-number *empty-terminalset*))
                   (setf (gethash kernel lalr-states-hash) destination-state)
                   (incf next-state-number)
                   (push destination-state states)
                   (push destination-state source-states)))
               (if (nonterminal? shift-symbol)
                 (push (cons shift-symbol destination-state)
                       (state-gotos source-state))
                 (each-shift-symbol-variant
                  #'(lambda (shift-symbol-variant)
                      (push (cons shift-symbol-variant (make-shift-transition destination-state))
                            (state-transitions source-state)))
                  grammar shift-symbol kernel-item-alist))))
         source-state :lalr-1)))
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
          (dolist (propagation (laitem-propagates dirty-laitem))
            (let ((dst-laitem (car propagation))
                  (mask (cdr propagation)))
              (let* ((old-dst-lookaheads (laitem-lookaheads dst-laitem))
                     (new-dst-lookaheads (terminalset-union old-dst-lookaheads (terminalset-intersection src-lookaheads mask))))
                (unless (terminalset-= old-dst-lookaheads new-dst-lookaheads)
                  (setf (laitem-lookaheads dst-laitem) new-dst-lookaheads)
                  (setf (gethash dst-laitem dirty-laitems) t))))))))
    
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
          (let ((lookaheads (terminalset-difference
                             (terminalset-intersection
                              (laitem-lookaheads laitem)
                              (general-production-constraint (item-production item) (item-dot item)))
                             (laitem-forbidden laitem))))
            (if (grammar-symbol-= (item-lhs item) *start-nonterminal*)
              (when (terminal-in-terminalset grammar *end-marker* lookaheads)
                (push (cons *end-marker* (make-accept-transition))
                      (state-transitions state)))
              (map-terminalset-reverse
               #'(lambda (lookahead)
                   (push (cons lookahead (make-reduce-transition (item-production item)))
                         (state-transitions state)))
               grammar
               lookaheads))))))
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
; Return true if ambiguities were found.
(defun report-and-fix-ambiguities (grammar stream)
  (let ((found-ambiguities nil))
    (dolist (state (grammar-states grammar))
      (labels
        
        ((report-ambiguity (transition-cons other-transition-conses)
           (unless found-ambiguities
             (setq found-ambiguities t)
             (format stream "~&Ambiguities:"))
           (write-char #\newline stream)
           (pprint-logical-block (stream nil)
             (format stream "S~D: ~W => " (state-number state) (car transition-cons))
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
          (setf (state-transitions state) (check transition-conses transition-conses)))))
    (when found-ambiguities
      (write-char #\newline stream))
    found-ambiguities))


; Remove the temporary item and laitem lists from the grammar's states.  This reduces the grammar's lisp
; heap usage but prevents it from being printed.
(defun clean-grammar (grammar)
  (when (grammar-items-hash grammar)
    (setf (grammar-items-hash grammar) nil)
    (dolist (state (grammar-states grammar))
      (setf (state-kernel state) nil)
      (setf (state-laitems state) nil))))


; Erase the existing parser, if any, for the given grammar.
(defun clear-parser (grammar)
  (setf (grammar-items-hash grammar) nil)
  (setf (grammar-states grammar) nil))


; Construct a LR or LALR parser in the given grammar.  kind should be :lalr-1, :lr-1, or :canonical-lr-1.
; Return true if ambiguities were found.
(defun compile-parser (grammar kind)
  (clear-parser grammar)
  (setf (grammar-items-hash grammar) (make-hash-table :test #'equal))
  (ecase kind
    (:lalr-1
     (add-all-lalr-states grammar)
     (propagate-lalr-lookaheads grammar))
    (:lr-1
     (add-all-lr-states grammar))
    (:canonical-lr-1
     (add-all-canonical-lr-states grammar)))
  (finish-transitions grammar)
  (report-and-fix-ambiguities grammar *error-output*))



; (cons (list <kind> <start-symbol> <grammar-source> <grammar-options>) <grammar>)
(defvar *make-and-compile-grammar-cache* (cons nil nil))

; Make the grammar and compile its parser.  kind should be :lalr-1, :lr-1, or :canonical-lr-1.
(defun make-and-compile-grammar (kind parametrization start-symbol grammar-source &rest grammar-options)
  (let ((key (list kind start-symbol grammar-source grammar-options))
        (cached-grammar (cdr *make-and-compile-grammar-cache*)))
    (if (and (equal key (car *make-and-compile-grammar-cache*))
             (grammar-parametrization-= parametrization cached-grammar))
      (progn
        (format *trace-output* "Re-using grammar ~S ~S ~S~%" kind start-symbol grammar-options)
        cached-grammar)
      (let* ((grammar (apply #'make-grammar parametrization start-symbol grammar-source grammar-options))
             (found-ambiguities (compile-parser grammar kind)))
        (setq *make-and-compile-grammar-cache*
              (if found-ambiguities
                (cons nil nil)
                (cons key grammar)))
        grammar))))


; Collapse states that have at most one possible reduction into forwarding states.
; DON'T DO THIS ON GRAMMARS THAT HAVE CONSTRAINTS AT THE TAIL END OF A PRODUCTION.
; Return the number of states optimized.
(defun forward-parser-states (grammar)
  (let ((n-forwarded-states 0))
    (dolist (state (grammar-states grammar))
      (let ((production (forwarding-state-production state)))
        (when production
          (setf (state-transitions state) (list (cons nil (make-reduce-transition production))))
          (incf n-forwarded-states))))
    n-forwarded-states))


;;; ------------------------------------------------------------------------------------------------------

(define-condition syntax-error (error)
                  ((message :reader syntax-error-message :initarg :message))
  (:report
   (lambda (condition stream)
     (format stream "Syntax error: ~A" (syntax-error-message condition)))))


(defun syntax-error (control-string &rest args)
  (error 'syntax-error :message (apply #'format nil control-string args)))


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
              (transition (state-transition state terminal)))
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
           (syntax-error "Parse error on ~S followed by ~S ..." token (ldiff input-rest (nthcdr 10 input-rest)))))))
    
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
    (flet ((add-signature (variant)
             (let ((signature (gethash variant action-signatures :undefined)))
               (unless (eq signature :undefined)
                 (setq symbol-exists t)
                 (when (assoc action-symbol signature :test #'eq)
                   (error "Attempt to redefine the type of action ~S on ~S" action-symbol variant))
                 (setf (gethash variant action-signatures)
                       (nconc signature (list (cons action-symbol type-expr))))))))
      (dolist (grammar-symbol grammar-symbols)
        (if (nonterminal? grammar-symbol)
          (progn
            (add-signature grammar-symbol)
            (dolist (production (rule-productions (grammar-rule grammar grammar-symbol)))
              (setf (production-actions production)
                    (nconc (production-actions production) (list (cons action-symbol nil))))))
          (let ((terminal-actions (grammar-terminal-actions grammar)))
            (assert-type grammar-symbol terminal)
            (dolist (variant (terminal-variants grammar grammar-symbol))
              (add-signature variant)
              (setf (gethash variant terminal-actions)
                    (nconc (gethash variant terminal-actions) (list (cons action-symbol nil)))))))))
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
(defun define-action (grammar production-name action-symbol type action-expr)
  (dolist (production (general-production-productions (grammar-general-production grammar production-name)))
    (let ((definition (assoc action-symbol (production-actions production) :test #'eq)))
      (cond
       ((null definition)
        (error "Attempt to define action ~S on ~S, which hasn't been declared yet" action-symbol production-name))
       ((cdr definition)
        (error "Duplicate definition of action ~S on ~S" action-symbol production-name))
       (t (setf (cdr definition) (make-action type action-expr)))))))


; Define action action-symbol, when called on the given terminal,
; to execute the given function, which should take a token as an input and
; produce a value of the proper type as output.
; The action should have been declared already.
(defun define-terminal-action (grammar terminal action-symbol action-function)
  (assert-type action-function function)
  (dolist (variant (terminal-variants grammar terminal))
    (let ((definition (assoc action-symbol (gethash variant (grammar-terminal-actions grammar)) :test #'eq)))
      (cond
       ((null definition)
        (error "Attempt to define action ~S on ~S, which hasn't been declared yet" action-symbol variant))
       ((cdr definition)
        (error "Duplicate definition of action ~S on ~S" action-symbol variant))
       (t (setf (cdr definition) action-function))))))



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
     ;When trace is non-null, type-stack contains the types of corresponding value-stack entries.
     (parse-step (state-stack value-stack type-stack input)
       (if (endp input)
         (parse-step-1 state-stack value-stack type-stack *end-marker* nil nil)
         (let ((token (first input)))
           (parse-step-1 state-stack value-stack type-stack (funcall token-terminal token) token (rest input)))))
     
     ;Same as parse-step except that the next input terminal has been determined already.
     ;input-rest contains the input tokens after the next token.
     (parse-step-1 (state-stack value-stack type-stack terminal token input-rest)
       (let* ((state (car state-stack))
              (transition (state-transition state terminal)))
         (when trace
           (format *trace-output* "S~D: ~@_" (state-number state))
           (print-values (reverse value-stack) (reverse type-stack) *trace-output*)
           (pprint-newline :mandatory *trace-output*))
         (if transition
           (case (transition-kind transition)
             (:shift
              (when trace
                (format *trace-output* "  shift ~W~:@_" terminal)
                (dolist (action-signature (grammar-symbol-signature grammar terminal))
                  (push (cdr action-signature) type-stack)))
              (dolist (action-function-binding (gethash terminal (grammar-terminal-actions grammar)))
                (push (funcall (cdr action-function-binding) token) value-stack))
              (parse-step (cons (transition-state transition) state-stack) value-stack type-stack input-rest))
             
             (:reduce
              (let ((production (transition-production transition)))
                (when trace
                  (write-string "  reduce " *trace-output*)
                  (if (eq trace :code)
                    (write production :stream *trace-output* :pretty t)
                    (print-production production *trace-output*))
                  (pprint-newline :mandatory *trace-output*))
                (let* ((state-stack (nthcdr (production-rhs-length production) state-stack))
                       (state (car state-stack))
                       (dst-state (assert-non-null
                                   (cdr (assoc (production-lhs production) (state-gotos state) :test *grammar-symbol-=*))))
                       (value-stack (funcall (production-evaluator production) value-stack)))
                  (when trace
                    (setq type-stack (nthcdr (production-n-action-args production) type-stack))
                    (dolist (action-signature (grammar-symbol-signature grammar (production-lhs production)))
                      (push (cdr action-signature) type-stack)))
                  (parse-step-1 (cons dst-state state-stack) value-stack type-stack terminal token input-rest))))
             
             (:accept
              (when trace
                (format *trace-output* "  accept~:@_"))
              (values
               (nreverse value-stack)
               (if trace
                 (nreverse type-stack)
                 (grammar-user-start-action-types grammar))))
             
             (t (error "Bad transition: ~S" transition)))
           (syntax-error "Parse error on ~S followed by ~S ..." token (ldiff input-rest (nthcdr 10 input-rest)))))))
    
    (parse-step (list (grammar-start-state grammar)) nil nil input)))

