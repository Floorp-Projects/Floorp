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
;;; Lexer grammar generator
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;


;;; A lexer grammar is an extension of a standard grammar that combines both parsing and combining
;;; characters into character classes.
;;;
;;; A lexer grammar is comprised of the following:
;;;   a start nonterminal;
;;;   a list of grammar productions, in which each terminal must be a character;
;;;   a list of character classes, where each class is a list of:
;;;     a nonterminal C;
;;;     an expression <set-expr> that denotes the set of characters in character class C;
;;;     a list of bindings, each containing:
;;;       an action name;
;;;       a lexer-action name;
;;;   a list of lexer-action bindings, each containing:
;;;     a lexer-action name;
;;;     the type of this lexer-action's value;
;;;     the name of a lisp function (char -> value) that performs the lexer-action on a character.
;;;
;;; Grammar productions may refer to character classes C as nonterminals.
;;;
;;; An expression <set-expr> can be any of the following:
;;;   C                                   The name of a previously defined character class.
;;;   every                               The set of all characters
;;;   (char1 char2 ... charn)             The set of characters {char1, char2, ..., charn}
;;;   (+ <set-expr1> ... <set-exprn>)     The set union of <set-expr1>, ..., <set-exprn>,
;;;                                       which should be disjoint.
;;;   (++ <set-expr1> ... <set-exprn>)    Same as +, but printed on separate lines.
;;;   (- <set-expr1> <set-expr2>)         The set of characters in <set-expr1> but not <set-expr2>;
;;;                                       <set-expr2> should be a subset of <set-expr1>.


;;; ------------------------------------------------------------------------------------------------------
;;; SETS OF CHARACTERS

;;; A character set is represented by an integer.
;;; The set may be infinite as long as its complement is finite.
;;; Bit n is set if the character with code n is a member of the set.
;;; The integer is negative if the set is infinite.


; Print the charset
(defun print-charset (charset &optional (stream t))
  (pprint-logical-block (stream (bitmap-to-ranges charset) :prefix "{" :suffix "}")
    (pprint-exit-if-list-exhausted)
    (loop
      (flet
        ((int-to-char (i)
           (if (or (eq i :infinity) (= i char-code-limit))
             :infinity
             (code-char i))))
        (let* ((range (pprint-pop))
               (lo (int-to-char (car range)))
               (hi (int-to-char (cdr range))))
          (write (if (eql lo hi) lo (list lo hi)) :stream stream :pretty t)
          (pprint-exit-if-list-exhausted)
          (format stream " ~:_"))))))


; Return the character set consisting of the single character char.
(declaim (inline char-charset))
(defun char-charset (char)
  (ash 1 (char-code char)))


; Return the character set consisting of adding char to the given charset.
(defun charset-add-char (charset char)
  (let ((i (char-code char)))
    (if (logbitp i charset)
      charset
      (logior charset (ash 1 i)))))


; Return the union of the two character sets, which should be disjoint.
(defun charset-union (charset1 charset2)
  (unless (zerop (logand charset1 charset2))
    (error "Union of overlapping character sets"))
  (logior charset1 charset2))


; Return the difference of the two character sets, the second of which should be
; a subset of the first.
(defun charset-difference (charset1 charset2)
  (unless (zerop (logandc1 charset1 charset2))
    (error "Difference of non-subset character sets"))
  (logandc2 charset1 charset2))


; Return true if the character set is empty.
(declaim (inline charset-empty?))
(defun charset-empty? (charset)
  (zerop charset))


; Return true if the character set is infinite.
(declaim (inline charset-infinite?))
(defun charset-infinite? (charset)
  (minusp charset))


; If the character set contains exactly one character, return that character;
; otherwise, return nil.
(defun charset-char (charset)
  (let ((hi (1- (integer-length charset))))
    (and (plusp charset) (= charset (ash 1 hi)) (code-char hi))))


; Return the highest character in the character set, which must be finite and nonempty.
(declaim (inline charset-highest-char))
(defun charset-highest-char (charset)
  (assert-true (plusp charset))
  (code-char (1- (integer-length charset))))


; Given a list of charsets, return a list of the largest possible
; charsets (called partitions) such that:
;   for any input charset C and partition P, either P is entirely contained in C or it is disjoint from C;
;   all partitions are mutually disjoint;
;   the union of all partitions is the infinite set of all characters.
(defun compute-partitions (charsets)
  (labels
    ((split-partitions (partitions charset)
       (mapcan #'(lambda (partition)
                   (remove-if #'zerop (list (logand partition charset) (logandc2 partition charset))))
               partitions))
     (partition< (partition1 partition2)
       (cond
        ((minusp partition1) nil)
        ((minusp partition2) t)
        (t (< partition1 partition2)))))
    (sort (reduce #'split-partitions charsets :initial-value '(-1))
          #'partition<)))


;;; ------------------------------------------------------------------------------------------------------
;;; LEXER-ACTIONS

(defstruct (lexer-action (:constructor make-lexer-action (name number type-expr function-name markup))
                         (:copier nil)
                         (:predicate lexer-action?))
  (name nil :type identifier :read-only t)             ;The action name to use for this lexer-action
  (number nil :type integer :read-only t)              ;Serial number of this lexer-action
  (type-expr nil :read-only t)                         ;A type expression that specifies the result type of list function function-name
  (function-name nil :type identifier :read-only t)    ;A lisp function (char -> value) that performs the lexer-action on a character
  (markup nil :type list :read-only t))                ;Markup template describing this lexer-action; replace '* with the nonterminal


(defun print-lexer-action (lexer-action &optional (stream t))
  (format stream "~@<~A ~@_~:I: ~<<<~;~W~;>>~:> ~_= ~<<~;#'~W~;>~:>~:>"
          (lexer-action-name lexer-action)
          (list (lexer-action-type-expr lexer-action))
          (list (lexer-action-function-name lexer-action))))


(defun depict-lexer-action (markup-stream lexer-action nonterminal)
  (dolist (markup-item (lexer-action-markup lexer-action))
    (if (eq markup-item '*)
      (depict-general-nonterminal markup-stream nonterminal)
      (depict-group markup-stream markup-item))))


;;; ------------------------------------------------------------------------------------------------------
;;; CHARCLASSES

(defstruct (charclass (:constructor make-charclass (nonterminal charset-source charset actions))
                      (:predicate charclass?))
  (nonterminal nil :type nonterminal :read-only t)    ;The nonterminal on the left-hand side of this production
  (charset-source nil :read-only t)                   ;The source expression for the charset
  (charset nil :type integer :read-only t)            ;The set of characters in this class
  (actions nil :type list :read-only t))              ;List of (action-name . lexer-action)


; Evaluate a <set-expr> whose syntax is given at the top of this file.
; Return the charset.
; charclasses-hash is a hash table of nonterminal -> charclass.
(defun eval-charset-expr (charclasses-hash expr)
  (cond
   ((null expr) 0)
   ((eq expr 'every) -1)
   ((symbolp expr)
    (charclass-charset
     (or (gethash expr charclasses-hash)
         (error "Character class ~S not defined" expr))))
   ((consp expr)
    (labels
      ((recursive-eval (expr)
         (eval-charset-expr charclasses-hash expr)))
      (case (car expr)
        ((+ ++) (reduce #'charset-union (cdr expr) :initial-value 0 :key #'recursive-eval))
        (- (unless (cdr expr)
             (error "Bad character set expression ~S" expr))
           (reduce #'charset-difference (cdr expr) :key #'recursive-eval))
        (t (reduce #'charset-union expr :key #'char-charset)))))
   (t (error "Bad character set expression ~S" expr))))


(defun print-charclass (charclass &optional (stream t))
  (pprint-logical-block (stream nil)
    (format stream "~W -> ~@_~:I" (charclass-nonterminal charclass))
    (print-charset (charclass-charset charclass) stream)
    (format stream " ~_")
    (pprint-fill stream (mapcar #'car (charclass-actions charclass)))))


; Emit markup for the lexer charset expression.
(defun depict-charset-source (markup-stream expr)
  (cond
   ((null expr) (error "Can't emit null charset expression"))
   ((eq expr 'every) (depict-general-nonterminal markup-stream ':any-character))
   ((symbolp expr) (depict-general-nonterminal markup-stream expr))
   ((consp expr)
    (case (car expr)
      ((+ ++) (depict-list markup-stream #'depict-charset-source (cdr expr) :separator " | "))
      (- (depict-charset-source markup-stream (second expr))
         (depict markup-stream " " :but-not " ")
         (depict-list markup-stream #'depict-charset-source (cddr expr) :separator " | "))
      (t (depict-list markup-stream #'depict-terminal expr :separator " | "))))
   (t (error "Bad character set expression ~S" expr))))


; Emit markup paragraphs for the lexer charclass.
(defun depict-charclass (markup-stream charclass)
  (let ((nonterminal (charclass-nonterminal charclass))
        (expr (charclass-charset-source charclass)))
    (if (and (consp expr) (eq (car expr) '++))
      (let* ((subexprs (cdr expr))
             (length (length subexprs)))
        (depict-paragraph (markup-stream ':grammar-lhs)
          (depict-general-nonterminal markup-stream nonterminal)
          (depict markup-stream " " ':derives-10))
        (dotimes (i length)
          (depict-paragraph (markup-stream (if (= i (1- length)) ':grammar-rhs-last ':grammar-rhs))
            (if (zerop i)
              (depict markup-stream ':tab3)
              (depict markup-stream "|" ':tab2))
            (depict-charset-source markup-stream (nth i subexprs)))))
      (depict-paragraph (markup-stream ':grammar-lhs-last)
        (depict-general-nonterminal markup-stream (charclass-nonterminal charclass))
        (depict markup-stream " " ':derives-10 " ")
        (depict-charset-source markup-stream expr)))))


;;; ------------------------------------------------------------------------------------------------------
;;; PARTITIONS

(defstruct (partition (:constructor make-partition (charset lexer-actions))
                      (:predicate partition?))
  (charset nil :type integer :read-only t)            ;The set of characters in this partition
  (lexer-actions nil :type list :read-only t))        ;List of lexer-actions needed on characters in this partition

(defconstant *default-partition-name* '$_other_) ;partition-name to use for characters not found in lexer-char-tokens


(defun print-partition (partition-name partition &optional (stream t))
  (pprint-logical-block (stream nil)
    (format stream "~W -> ~@_~:I" partition-name)
    (print-charset (partition-charset partition) stream)
    (format stream " ~_")
    (pprint-fill stream (mapcar #'lexer-action-name (partition-lexer-actions partition)))))


;;; ------------------------------------------------------------------------------------------------------
;;; LEXER


(defstruct (lexer (:constructor allocate-lexer)
                  (:copier nil)
                  (:predicate lexer?))
  (lexer-actions nil :type hash-table :read-only t)    ;Hash table of lexer-action-name -> lexer-action
  (charclasses nil :type list :read-only t)            ;List of charclasses in the order in which they were given
  (charclasses-hash nil :type hash-table :read-only t) ;Hash table of nonterminal -> charclass
  (char-tokens nil :type hash-table :read-only t)      ;Hash table of character -> (character or partition-name)
  (partition-names nil :type list :read-only t)        ;List of partition names in the order in which they were created
  (partitions nil :type hash-table :read-only t)       ;Hash table of partition-name -> partition
  (grammar nil :type (or null grammar))                ;Grammar that accepts exactly one lexer token
  (metagrammar nil :type (or null metagrammar)))       ;Grammar that accepts the longest input sequence that forms a token


; Return a function (character -> terminal) that classifies an input character
; as either itself or a partition-name.
; If the returned function is called on a non-character, it returns its input unchanged.
(defun lexer-classifier (lexer)
  (let ((char-tokens (lexer-char-tokens lexer)))
    #'(lambda (char)
        (if (characterp char)
          (gethash char char-tokens *default-partition-name*)
          char))))


; Return the charclass that defines the given lexer nonterminal or nil if none.
(defun lexer-charclass (lexer nonterminal)
  (gethash nonterminal (lexer-charclasses-hash lexer)))


; Return the charset of all characters that appear as terminals in grammar-source.
(defun grammar-singletons (grammar-source)
  (assert-type grammar-source (list (tuple t (list t) identifier)))
  (let ((singletons 0))
    (dolist (production-source grammar-source)
      (dolist (grammar-symbol (second production-source))
        (when (characterp grammar-symbol)
          (setq singletons (charset-add-char singletons grammar-symbol)))))
    singletons))


; Return the list of all lexer-action-names that appear in at least one charclass of which this
; partition is a subset.
(defun collect-lexer-action-names (charclasses partition)
  (let ((lexer-action-names nil))
    (dolist (charclass charclasses)
      (unless (zerop (logand (charclass-charset charclass) partition))
        (dolist (action (charclass-actions charclass))
          (pushnew (cdr action) lexer-action-names))))
    (sort lexer-action-names #'< :key #'lexer-action-number)))


; Make a lexer structure corresponding to a grammar with the given source.
; charclasses-source is a list of character classes, where each class is a list of:
;     a nonterminal C;
;     an expression <set-expr> that denotes the set of characters in character class C;
;     a list of bindings, each containing:
;       an action name;
;       a lexer-action name.
; lexer-actions-source is a list of lexer-action bindings, each containing:
;     a lexer-action name;
;     the type of this lexer-action's value;
;     the name of a lisp function (char -> value) that performs the lexer-action on a character.
; This does not make the lexer's grammar; use make-lexer-and-grammar for that.
(defun make-lexer (charclasses-source lexer-actions-source grammar-source)
  (assert-type charclasses-source (list (tuple nonterminal t (list (tuple identifier identifier)))))
  (assert-type lexer-actions-source (list (tuple identifier t identifier list)))
  (let ((lexer-actions (make-hash-table :test #'eq))
        (charclasses nil)
        (charclasses-hash (make-hash-table :test *grammar-symbol-=*))
        (charsets nil)
        (singletons (grammar-singletons grammar-source)))
    (let ((lexer-action-number 0))
      (dolist (lexer-action-source lexer-actions-source)
        (let ((name (first lexer-action-source))
              (type-expr (second lexer-action-source))
              (function (third lexer-action-source))
              (markup (fourth lexer-action-source)))
          (when (gethash name lexer-actions)
            (error "Attempt to redefine lexer action ~S" name))
          (setf (gethash name lexer-actions)
                (make-lexer-action name (incf lexer-action-number) type-expr function markup)))))
    
    (dolist (charclass-source charclasses-source)
      (let ((nonterminal (first charclass-source))
            (charset (eval-charset-expr charclasses-hash (ensure-proper-form (second charclass-source))))
            (actions 
             (mapcar #'(lambda (action-source)
                         (let* ((lexer-action-name (second action-source))
                                (lexer-action (gethash lexer-action-name lexer-actions)))
                           (unless lexer-action
                             (error "Unknown lexer-action ~S" lexer-action-name))
                           (cons (first action-source) lexer-action)))
                     (third charclass-source))))
        (when (gethash nonterminal charclasses-hash)
          (error "Attempt to redefine character class ~S" nonterminal))
        (when (charset-empty? charset)
          (error "Empty character class ~S" nonterminal))
        (let ((charclass (make-charclass nonterminal (second charclass-source) charset actions)))
          (push charclass charclasses)
          (setf (gethash nonterminal charclasses-hash) charclass)
          (push charset charsets))))
    (setq charclasses (nreverse charclasses))
    (bitmap-each-bit #'(lambda (i) (push (ash 1 i) charsets))
                     singletons)
    (let ((char-tokens (make-hash-table :test #'eql))
          (partition-names nil)
          (partitions (make-hash-table :test #'eq))
          (current-partition-number 0))
      (dolist (partition (compute-partitions charsets))
        (let ((singleton (charset-char partition)))
          (cond
           (singleton (setf (gethash singleton char-tokens) singleton))
           ((charset-infinite? partition)
            (push *default-partition-name* partition-names)
            (setf (gethash *default-partition-name* partitions)
                  (make-partition partition (collect-lexer-action-names charclasses partition))))
           (t (let ((token (intern (format nil "$_CHARS~D_" (incf current-partition-number)))))
                (bitmap-each-bit #'(lambda (i)
                                     (setf (gethash (code-char i) char-tokens) token))
                                 partition)
                (push token partition-names)
                (setf (gethash token partitions)
                      (make-partition partition (collect-lexer-action-names charclasses partition))))))))
      (allocate-lexer
       :lexer-actions lexer-actions
       :charclasses charclasses
       :charclasses-hash charclasses-hash
       :char-tokens char-tokens
       :partition-names (nreverse partition-names)
       :partitions partitions))))


(defun print-lexer (lexer &optional (stream t))
  (let* ((lexer-actions (lexer-lexer-actions lexer))
         (lexer-action-names (sort (hash-table-keys lexer-actions) #'<
                                   :key #'(lambda (lexer-action-name)
                                            (lexer-action-number (gethash lexer-action-name lexer-actions)))))
         (charclasses (lexer-charclasses lexer))
         (partition-names (lexer-partition-names lexer))
         (partitions (lexer-partitions lexer))
         (singletons nil))
    
    (when lexer-action-names
      (pprint-logical-block (stream lexer-action-names)
        (format stream "Lexer Actions:~2I")
        (loop
          (pprint-newline :mandatory stream)
          (let ((lexer-action (gethash (pprint-pop) lexer-actions)))
            (print-lexer-action lexer-action stream))
          (pprint-exit-if-list-exhausted)))
      (pprint-newline :mandatory stream)
      (pprint-newline :mandatory stream))
    
    (when charclasses
      (pprint-logical-block (stream charclasses)
        (format stream "Charclasses:~2I")
        (loop
          (pprint-newline :mandatory stream)
          (let ((charclass (pprint-pop)))
            (print-charclass charclass stream))
          (pprint-exit-if-list-exhausted)))
      (pprint-newline :mandatory stream)
      (pprint-newline :mandatory stream))
    
    (when partition-names
      (pprint-logical-block (stream partition-names)
        (format stream "Partitions:~2I")
        (loop
          (pprint-newline :mandatory stream)
          (let ((partition-name (pprint-pop)))
            (print-partition partition-name (gethash partition-name partitions) stream))
          (pprint-exit-if-list-exhausted)))
      (pprint-newline :mandatory stream)
      (pprint-newline :mandatory stream))
    
    (maphash
     #'(lambda (char char-or-partition)
         (if (eql char char-or-partition)
           (push char singletons)
           (assert-type char-or-partition identifier)))
     (lexer-char-tokens lexer))
    (setq singletons (sort singletons #'char<))
    (when singletons
      (format stream "Singletons: ~@_~<~@{~W ~:_~}~:>~:@_~:@_" singletons))))
    

(defmethod print-object ((lexer lexer) stream)
  (print-unreadable-object (lexer stream :identity t)
    (write-string "lexer" stream)))


;;; ------------------------------------------------------------------------------------------------------


; Return two values:
;   extra grammar productions that define the character class nonterminals out of characters and tokens;
;   extra commands that:
;     define the partitions used in this lexer;
;     define the actions of these productions.
(defun lexer-grammar-and-commands (lexer)
  (labels
    ((component-partitions (charset partitions)
       (if (charset-empty? charset)
         partitions
         (let* ((partition-name (if (charset-infinite? charset)
                                  *default-partition-name*
                                  (gethash (charset-highest-char charset) (lexer-char-tokens lexer))))
                (partition (gethash partition-name (lexer-partitions lexer))))
           (component-partitions (charset-difference charset (partition-charset partition))
                                 (cons partition partitions))))))
    (let ((productions nil)
          (commands nil))
      (dolist (charclass (lexer-charclasses lexer))
        (let ((nonterminal (charclass-nonterminal charclass))
              (production-number 0))
          (dolist (action (charclass-actions charclass))
            (let ((lexer-action (cdr action)))
              (push (list 'declare-action (car action) nonterminal (lexer-action-type-expr lexer-action)) commands)))
          (do ((charset (charclass-charset charclass)))
              ((charset-empty? charset))
            (let* ((partition-name (if (charset-infinite? charset)
                                     *default-partition-name*
                                     (gethash (charset-highest-char charset) (lexer-char-tokens lexer))))
                   (partition-charset (if (characterp partition-name)
                                        (char-charset partition-name)
                                        (partition-charset (gethash partition-name (lexer-partitions lexer)))))
                   (production-name (intern (format nil "~A-~D" nonterminal (incf production-number)))))
              (push (list nonterminal (list partition-name) production-name) productions)
              (dolist (action (charclass-actions charclass))
                (let* ((lexer-action (cdr action))
                       (body (if (characterp partition-name)
                               (let* ((lexer-action-function (lexer-action-function-name lexer-action))
                                      (result (funcall lexer-action-function partition-name)))
                                 (typecase result
                                   (integer result)
                                   (character result)
                                   ((eql nil) 'false)
                                   ((eql t) 'true)
                                   (t (error "Cannot infer the type of ~S's result ~S" lexer-action-function result))))
                               (list (lexer-action-name lexer-action) partition-name))))
                  (push (list 'action (car action) production-name body nil) commands)))
              (setq charset (charset-difference charset partition-charset))))))
      
      (let ((partition-commands
             (mapcan
              #'(lambda (partition-name)
                  (mapcan #'(lambda (lexer-action)
                              (let ((lexer-action-name (lexer-action-name lexer-action)))
                                (list
                                 (list 'declare-action lexer-action-name partition-name (lexer-action-type-expr lexer-action))
                                 (list 'terminal-action lexer-action-name partition-name (lexer-action-function-name lexer-action)))))
                          (partition-lexer-actions (gethash partition-name (lexer-partitions lexer)))))
              (lexer-partition-names lexer))))
        (values
         (nreverse productions)
         (nconc partition-commands (nreverse commands)))))))


; Make a lexer and grammar from the given source.
; kind should be either :lalr-1 or :lr-1.
; charclasses-source is a list of character classes, and
; lexer-actions-source is a list of lexer-action bindings; see make-lexer.
; start-symbol is the grammar's start symbol, and grammar-source is its source.
; Return two values:
;   the lexer (including the grammar in its grammar field);
;   list of extra commands that:
;     define the partitions used in this lexer;
;     define the actions of these productions.
(defun make-lexer-and-grammar (kind charclasses-source lexer-actions-source parametrization start-symbol grammar-source &optional excluded-nonterminals-source)
  (let ((lexer (make-lexer charclasses-source lexer-actions-source grammar-source)))
    (multiple-value-bind (extra-grammar-source extra-commands) (lexer-grammar-and-commands lexer)
      (let ((grammar (make-and-compile-grammar kind parametrization start-symbol
                                               (append extra-grammar-source grammar-source) excluded-nonterminals-source)))
        (setf (lexer-grammar lexer) grammar)
        (values lexer extra-commands)))))


; Parse the input string to produce a list of action results.
; If trace is:
;   nil,     don't print trace information
;   :code,   print trace information, including action code
;   other    print trace information
; Return two values:
;   the list of action results;
;   the list of action results' types.
(defun lexer-parse (lexer string &key trace)
  (let ((in (coerce string 'list)))
    (action-parse (lexer-grammar lexer) (lexer-classifier lexer) in :trace trace)))


; Same as lexer-parse except that also print the action results nicely.
(defun lexer-pparse (lexer string &key (stream t) trace)
  (multiple-value-bind (results types) (lexer-parse lexer string :trace trace)
    (print-values results types stream)
    (terpri stream)
    (values results types)))


; Compute the lexer grammar's metagrammar.
(defun set-up-lexer-metagrammar (lexer)
  (setf (lexer-metagrammar lexer) (make-metagrammar (lexer-grammar lexer))))



; Parse the input string into elements, where each element is the longest
; possible string of input characters that is accepted by the grammar.
; The grammar's terminals are all characters that may appear in the input
; string plus the symbol $END which is inserted after the last character of
; the string.
; Return the list of lists of action results of the elements.
; If trace is:
;   nil,     don't print trace information
;   :code,   print trace information, including action code
;   other    print trace information
; Return two values:
;   the list of lists of action results;
;   the list of action results' types.  Each of the lists of action results has
;     this type signature.
(defun lexer-metaparse (lexer string &key trace)
  (let ((metagrammar (lexer-metagrammar lexer)))
    (do ((in (append (coerce string 'list) '($end)))
         (results-lists nil))
        ((endp in) (values (nreverse results-lists) (grammar-user-start-action-types (metagrammar-grammar metagrammar))))
      (multiple-value-bind (results in-rest) (action-metaparse metagrammar (lexer-classifier lexer) in :trace trace)
        (setq in in-rest)
        (push results results-lists)))))


; Same as lexer-metaparse except that also print the action results nicely.
(defun lexer-pmetaparse (lexer string &key (stream t) trace)
  (multiple-value-bind (results-lists types) (lexer-metaparse lexer string :trace trace)
    (pprint-logical-block (stream results-lists)
      (pprint-exit-if-list-exhausted)
      (loop
        (print-values (pprint-pop) types stream :prefix "(" :suffix ")")
        (pprint-exit-if-list-exhausted)
        (format stream " ~_")))
    (terpri stream)
    (values results-lists types)))

