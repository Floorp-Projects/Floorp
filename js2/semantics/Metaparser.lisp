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
;;; Finite-state machine generator
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


;;; ------------------------------------------------------------------------------------------------------
;;; METATRANSITION

(defstruct (metatransition (:constructor make-metatransition (next-metastate pre-productions post-productions)))
  (next-metastate nil :read-only t)     ;Next metastate to enter or nil if this is an accept transition
  (pre-productions nil :read-only t)    ;List of productions reduced by this transition (in order from first to last) before the shift
  (post-productions nil :read-only t))  ;List of productions reduced by this transition (in order from first to last) after the shift

  
;;; ------------------------------------------------------------------------------------------------------
;;; METASTATE

;;; A metastate is a list of states that represents a possible stack that the
;;; LALR(1) parser may encounter.
(defstruct (metastate (:constructor make-metastate (stack number transitions)))
  (stack nil :type list :read-only t)                 ;List of states that comprises a possible stack
  (number nil :type integer :read-only t)             ;Serial number of this metastate
  (transitions nil :type simple-vector :read-only t)) ;Array, indexed by terminal numbers, of either nil or metatransition structures

(declaim (inline metastate-transition))
(defun metastate-transition (metastate terminal-number)
  (svref (metastate-transitions metastate) terminal-number))


(defun print-metastate (metastate metagrammar &optional (stream t))
  (let ((grammar (metagrammar-grammar metagrammar)))
    (pprint-logical-block (stream nil)
      (format stream "M~D:~2I ~@_~<~@{S~D ~:_~}~:>~:@_"
              (metastate-number metastate)
              (nreverse (mapcar #'state-number (metastate-stack metastate))))
      (let ((transitions (metastate-transitions metastate)))
        (dotimes (terminal-number (length transitions))
          (let ((transition (svref transitions terminal-number))
                (terminal (svref (grammar-terminals grammar) terminal-number)))
            (when transition
              (let ((next-metastate (metatransition-next-metastate transition)))
                (pprint-logical-block (stream nil)
                  (format stream "~W ==> ~@_~:I~:[accept~;M~:*~D~] ~_"
                          terminal
                          (and next-metastate (metastate-number next-metastate)))
                  (pprint-fill stream (mapcar #'production-name (metatransition-pre-productions transition)))
                  (format stream " ~@_")
                  (pprint-fill stream (mapcar #'production-name (metatransition-post-productions transition))))
                (pprint-newline :mandatory stream)))))))))


(defmethod print-object ((metastate metastate) stream)
  (print-unreadable-object (metastate stream)
    (format stream "metastate S~D" (metastate-number metastate))))


;;; ------------------------------------------------------------------------------------------------------
;;; METAGRAMMAR

(defstruct (metagrammar (:constructor allocate-metagrammar))
  (grammar nil :type grammar :read-only t)          ;The grammar to which this metagrammar corresponds
  (metastates nil :type list :read-only t)          ;List of metastates ordered by metastate numbers
  (start nil :type metastate :read-only t))         ;The start metastate


(defun make-metagrammar (grammar)
  (let* ((terminals (grammar-terminals grammar))
         (n-terminals (length terminals))
         (metastates-hash (make-hash-table :test #'equal))    ;Hash table of (list of state) -> metastate
         (metastates nil)
         (metastate-number -1))
    (labels
      (;Return the stack after applying the given reduction production.
       (apply-reduction-production (stack production)
         (let* ((stack (nthcdr (production-rhs-length production) stack))
                (state (first stack))
                (dst-state (assert-non-null
                            (cdr (assoc (production-lhs production) (state-gotos state) :test *grammar-symbol-=*))))
                (dst-stack (cons dst-state stack)))
           (if (member dst-state stack :test #'eq)
             (error "This grammar cannot be represented by a FSM.  Stack: ~S" dst-stack)
             dst-stack)))
       
       (get-metatransition (stack terminal productions)
         (let* ((state (first stack))
                (transition (cdr (assoc terminal (state-transitions state) :test *grammar-symbol-=*))))
           (when transition
             (case (transition-kind transition)
               (:shift
                (multiple-value-bind (metastate forwarding-productions) (get-metastate (transition-state transition) stack t)
                  (make-metatransition metastate (nreverse productions) forwarding-productions)))
               (:reduce
                (let ((production (transition-production transition)))
                  (get-metatransition (apply-reduction-production stack production) terminal (cons production productions))))
               (:accept (make-metatransition nil (nreverse productions) nil))
               (t (error "Bad transition: ~S" transition))))))
       
       ;Return the metastate corresponding to the state stack (stack-top . stack-rest).  Construct a new
       ;metastate if necessary.
       ;If simplify is true and stack-top is a state for which every outgoing transition is the same
       ;reduction, return two values:
       ;  the metastate reached by following that reduction (doing it recursively if needed)
       ;  a list of reduction productions followed this way.
       (get-metastate (stack-top stack-rest simplify)
         (let* ((stack (cons stack-top stack-rest))
                (existing-metastate (gethash stack metastates-hash)))
           (cond
            (existing-metastate (values existing-metastate nil))
            ((member stack-top stack-rest :test #'eq)
             (error "This grammar cannot be represented by a FSM.  Stack: ~S" stack))
            (t (let ((forwarding-production (and simplify (forwarding-state-production stack-top))))
                 (if forwarding-production
                   (let ((stack (apply-reduction-production stack forwarding-production)))
                     (multiple-value-bind (metastate forwarding-productions) (get-metastate (car stack) (cdr stack) simplify)
                       (values metastate (cons forwarding-production forwarding-productions))))
                   (let* ((transitions (make-array n-terminals :initial-element nil))
                          (metastate (make-metastate stack (incf metastate-number) transitions)))
                     (setf (gethash stack metastates-hash) metastate)
                     (push metastate metastates)
                     (dotimes (n n-terminals)
                       (setf (svref transitions n)
                             (get-metatransition stack (svref terminals n) nil)))
                     (values metastate nil)))))))))
      
      (let ((start-metastate (get-metastate (grammar-start-state grammar) nil nil)))
        (allocate-metagrammar :grammar grammar
                              :metastates (nreverse metastates)
                              :start start-metastate)))))


; Print the metagrammar nicely.
(defun print-metagrammar (metagrammar &optional (stream t) &key (grammar t) (details t))
  (pprint-logical-block (stream nil)
    (when grammar
      (print-grammar (metagrammar-grammar metagrammar) stream :details details))
    
    ;Print the metastates.
    (format stream "Start metastate: ~@_M~D~:@_~:@_" (metastate-number (metagrammar-start metagrammar)))
    (pprint-logical-block (stream (metagrammar-metastates metagrammar))
      (pprint-exit-if-list-exhausted)
      (format stream "Metastates:~2I~:@_")
      (loop
        (print-metastate (pprint-pop) metagrammar stream)
        (pprint-exit-if-list-exhausted)
        (pprint-newline :mandatory stream))))
  (pprint-newline :mandatory stream))


(defmethod print-object ((metagrammar metagrammar) stream)
  (print-unreadable-object (metagrammar stream :identity t)
    (write-string "metagrammar" stream)))


; Find the longest possible prefix of the input list of tokens that is accepted by the
; grammar.  Parse that prefix and return two values:
;   the list of action results;
;   the tail of the input list of tokens remaining to be parsed.
; Signal an error if no prefix of the input list is accepted by the grammar.
;
; token-terminal is a function that returns a terminal symbol when given an input token.
; If trace is:
;   nil,     don't print trace information
;   :code,   print trace information, including action code
;   other    print trace information
(defun action-metaparse (metagrammar token-terminal input &key trace)
  (if trace
    (trace-action-metaparse metagrammar token-terminal input trace)
    (let ((grammar (metagrammar-grammar metagrammar)))
      (labels
        ((transition-value-stack (value-stack productions)
           (dolist (production productions)
             (setq value-stack (funcall (production-evaluator production) value-stack)))
           value-stack)
         
         (cut (input good-metastate good-input good-value-stack)
           (unless good-metastate
             (syntax-error "Parse error on ~S ..." (ldiff input (nthcdr 10 input))))
           (let ((last-metatransition (metastate-transition good-metastate *end-marker-terminal-number*)))
             (assert-true (null (metatransition-next-metastate last-metatransition)))
             (assert-true (null (metatransition-post-productions last-metatransition)))
             (values
              (reverse (transition-value-stack good-value-stack (metatransition-pre-productions last-metatransition)))
              good-input))))
        
        (do ((metastate (metagrammar-start metagrammar))
             (input input (cdr input))
             (value-stack nil)
             (last-good-metastate nil)
             last-good-input
             last-good-value-stack)
            (nil)
          (when (metastate-transition metastate *end-marker-terminal-number*)
            (setq last-good-metastate metastate)
            (setq last-good-input input)
            (setq last-good-value-stack value-stack))
          (when (endp input)
            (return (cut input last-good-metastate last-good-input last-good-value-stack)))
          (let* ((token (first input))
                 (terminal (funcall token-terminal token))
                 (terminal-number (terminal-number grammar terminal))
                 (metatransition (metastate-transition metastate terminal-number)))
            (unless metatransition
              (return (cut input last-good-metastate last-good-input last-good-value-stack)))
            (setq value-stack (transition-value-stack value-stack (metatransition-pre-productions metatransition)))
            (dolist (action-function-binding (gethash terminal (grammar-terminal-actions grammar)))
              (push (funcall (cdr action-function-binding) token) value-stack))
            (setq value-stack (transition-value-stack value-stack (metatransition-post-productions metatransition)))
            (setq metastate (metatransition-next-metastate metatransition))))))))


; Same as action-parse, but with tracing information
; If trace is:
;   :code,   print trace information, including action code
;   other    print trace information
(defun trace-action-metaparse (metagrammar token-terminal input trace)
  (let
    ((grammar (metagrammar-grammar metagrammar)))
    (labels
      ((print-stacks (value-stack type-stack)
         (write-string "    " *trace-output*)
         (if value-stack
           (print-values (reverse value-stack) (reverse type-stack) *trace-output*)
           (write-string "empty" *trace-output*))
         (pprint-newline :mandatory *trace-output*))
       
       (transition-value-stack (value-stack type-stack productions)
         (dolist (production productions)
           (write-string "  reduce " *trace-output*)
           (if (eq trace :code)
             (write production :stream *trace-output* :pretty t)
             (print-production production *trace-output*))
           (pprint-newline :mandatory *trace-output*)
           (setq value-stack (funcall (production-evaluator production) value-stack))
           (setq type-stack (nthcdr (production-n-action-args production) type-stack))
           (dolist (action-signature (grammar-symbol-signature grammar (production-lhs production)))
             (push (cdr action-signature) type-stack))
           (print-stacks value-stack type-stack))
         (values value-stack type-stack))
       
       (cut (input good-metastate good-input good-value-stack good-type-stack)
         (unless good-metastate
           (syntax-error "Parse error on ~S ..." (ldiff input (nthcdr 10 input))))
         (let ((last-metatransition (metastate-transition good-metastate *end-marker-terminal-number*)))
           (assert-true (null (metatransition-next-metastate last-metatransition)))
           (assert-true (null (metatransition-post-productions last-metatransition)))
           (format *trace-output* "cut to M~D~:@_" (metastate-number good-metastate))
           (print-stacks good-value-stack good-type-stack)
           (pprint-newline :mandatory *trace-output*)
           (values
            (reverse (transition-value-stack good-value-stack good-type-stack (metatransition-pre-productions last-metatransition)))
            good-input))))
      
      (do ((metastate (metagrammar-start metagrammar))
           (input input (cdr input))
           (value-stack nil)
           (type-stack nil)
           (last-good-metastate nil)
           last-good-input
           last-good-value-stack
           last-good-type-stack)
          (nil)
        (format *trace-output* "M~D" (metastate-number metastate))
        (when (metastate-transition metastate *end-marker-terminal-number*)
          (write-string " (good)" *trace-output*)
          (setq last-good-metastate metastate)
          (setq last-good-input input)
          (setq last-good-value-stack value-stack)
          (setq last-good-type-stack type-stack))
        (write-string ": " *trace-output*)
        (when (endp input)
          (return (cut input last-good-metastate last-good-input last-good-value-stack last-good-type-stack)))
        (let* ((token (first input))
               (terminal (funcall token-terminal token))
               (terminal-number (terminal-number grammar terminal))
               (metatransition (metastate-transition metastate terminal-number)))
          (unless metatransition
            (format *trace-output* "shift ~W: " terminal)
            (return (cut input last-good-metastate last-good-input last-good-value-stack last-good-type-stack)))
          (format *trace-output* "transition to M~D~:@_" (metastate-number (metatransition-next-metastate metatransition)))
          (multiple-value-setq (value-stack type-stack)
            (transition-value-stack value-stack type-stack (metatransition-pre-productions metatransition)))
          (dolist (action-function-binding (gethash terminal (grammar-terminal-actions grammar)))
            (push (funcall (cdr action-function-binding) token) value-stack))
          (dolist (action-signature (grammar-symbol-signature grammar terminal))
            (push (cdr action-signature) type-stack))
          (format *trace-output* "shift ~W~:@_" terminal)
          (print-stacks value-stack type-stack)
          (multiple-value-setq (value-stack type-stack)
            (transition-value-stack value-stack type-stack (metatransition-post-productions metatransition)))
          (setq metastate (metatransition-next-metastate metatransition)))))))


; Compute all representative strings of terminals such that, for each such string S:
;   S is rejected by the grammar's language;
;   all prefixes of S are also rejected by the grammar's language;
;   for any S and all strings of terminals T, the concatenated string ST is also
;     rejected by the grammar's language;
;   no string S1 is a prefix of (or equal to) another string S2.
; Often there are infinitely many such strings S, so only output one for each illegal
; metaparser transition.
; Return a list of S's, where each S is itself a list of terminals.
(defun compute-illegal-strings (metagrammar)
  (let* ((grammar (metagrammar-grammar metagrammar))
         (terminals (grammar-terminals grammar))
         (n-terminals (length terminals))
         (metastates (metagrammar-metastates metagrammar))
         (n-metastates (length metastates))
         (visited-metastates (make-array n-metastates :element-type 'bit :initial-element 0))
         (illegal-strings nil))
    (labels
      ((visit (metastate reversed-string)
         (let ((metastate-number (metastate-number metastate)))
           (when (= (sbit visited-metastates metastate-number) 0)
             (setf (sbit visited-metastates metastate-number) 1)
             (let ((metatransitions (metastate-transitions metastate)))
               ;If there is a transition for the end marker from this state, then string
               ;is accepted by the language, so cut off the search.
               (unless (svref metatransitions *end-marker-terminal-number*)
                 (dotimes (terminal-number n-terminals)
                   (unless (= terminal-number *end-marker-terminal-number*)
                     (let ((metatransition (svref metatransitions terminal-number))
                           (reversed-string (cons (svref terminals terminal-number) reversed-string)))
                       (if metatransition
                         (visit (metatransition-next-metastate metatransition) reversed-string)
                         (push (reverse reversed-string) illegal-strings)))))))))))
      
      (visit (metagrammar-start metagrammar) nil)
      (nreverse illegal-strings))))


; Compute and print illegal strings of terminals.  See compute-illegal-strings.
(defun print-illegal-strings (metagrammar &optional (stream t))
  (pprint-logical-block (stream (compute-illegal-strings metagrammar))
    (format stream "Illegal strings:~2I")
    (loop
      (pprint-exit-if-list-exhausted)
      (pprint-newline :mandatory stream)
      (pprint-fill stream (pprint-pop))))
  (pprint-newline :mandatory stream))
