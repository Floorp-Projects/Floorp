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
;;; Handy lisp utilities
;;;
;;; Waldemar Horwat (waldemar@netscape.com)
;;;


;;; ------------------------------------------------------------------------------------------------------
;;; MCL FIXES


(setq *print-right-margin* 150)

;;; Fix name-char and char-name.
#+mcl
(locally
  (declare (optimize (speed 3) (safety 0) (debug 1)))
  (eval-when (:compile-toplevel :load-toplevel :execute)
    (setq *warn-if-redefine* nil)
    (setq *warn-if-redefine-kernel* nil))
  
  (defun char-name (c)  
    (dolist (e ccl::*name-char-alist*)
      (declare (list e))    
      (when (eq c (cdr e))
        (return-from char-name (car e))))
    (let ((code (char-code c)))
      (declare (fixnum code))
      (cond ((< code #x100)
             (unless (and (>= code 32) (<= code 216) (/= code 127))
               (format nil "x~2,'0X" code)))
            (t (format nil "u~4,'0X" code)))))
  
  (defun name-char (name)
    (if (characterp name)
      name
      (let* ((name (string name))
             (namelen (length name)))
        (declare (fixnum namelen))
        (or (cdr (assoc name ccl::*name-char-alist* :test #'string-equal))
            (if (= namelen 1)
              (char name 0)
              (when (>= namelen 2)
                (flet
                  ((number-char (name base lg-base)
                     (let ((n 0))
                       (dotimes (i (length name) (code-char n))
                         (let ((code (digit-char-p (char name i) base)))
                           (if code
                             (setq n (logior code (ash n lg-base)))
                             (return)))))))
                  (case (char name 0)
                    (#\^
                     (when (= namelen 2)
                       (code-char (the fixnum (logxor (the fixnum (char-code (char-upcase (char name 1)))) #x40)))))
                    ((#\x #\X #\u #\U)
                     (number-char (subseq name 1) 16 4))
                    ((#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7)
                     (number-char name 8 3))))))))))
  
  (eval-when (:compile-toplevel :load-toplevel :execute)
    (setq *warn-if-redefine* t)
    (setq *warn-if-redefine-kernel* t)))



;;; ------------------------------------------------------------------------------------------------------
;;; READER SYNTAX

; Define #?num to produce a character with code given by the hexadecimal number num.
; (This is a portable extension; the #\u syntax installed above does the same thing
; but is not portable.)
(set-dispatch-macro-character
 #\# #\?
 #'(lambda (stream subchar arg)
     (declare (ignore subchar arg))
     (let ((*read-base* 16))
       (code-char (read stream t nil t)))))


;;; ------------------------------------------------------------------------------------------------------
;;; MACROS

; (list*-bind (var1 var2 ... varn) expr body):
;   evaluates expr to obtain a value v;
;   binds var1, var2, ..., varn such that (list* var1 var2 ... varn) is equal to v;
;   evaluates body with these bindings;
;   returns the result values from the body.
(defmacro list*-bind ((var1 &rest vars) expr &body body)
  (labels
    ((gen-let*-bindings (var1 vars expr)
       (if vars
         (let ((expr-var (gensym "REST")))
           (list*
            (list expr-var expr)
            (list var1 (list 'car expr-var))
            (gen-let*-bindings (car vars) (cdr vars) (list 'cdr expr-var))))
         (list
          (list var1 expr)))))
    (list* 'let* (gen-let*-bindings var1 vars expr) body)))

(set-pprint-dispatch '(cons (member list*-bind))
                     (pprint-dispatch '(multiple-value-bind () ())))


; (multiple-value-map-bind (var1 var2 ... varn) f (src1 src2 ... srcm) body)
;   evaluates src1, src2, ..., srcm to obtain lists l1, l2, ..., lm;
;   calls f on corresponding elements of lists l1, ..., lm; each such call should return n values v1 v2 ... vn;
;   binds var1, var2, ..., varn such var1 is the list of all v1's, var2 is the list of all v2's, etc.;
;   evaluates body with these bindings;
;   returns the result values from the body.
(defmacro multiple-value-map-bind ((&rest vars) f (&rest srcs) &body body)
  (let ((n (length vars))
        (m (length srcs))
        (fun (gensym "F"))
        (ss nil)
        (vs nil)
        (accumulators nil))
    (dotimes (i n)
      (push (gensym "V") vs)
      (push (gensym "ACC") accumulators))
    (dotimes (i m)
      (push (gensym "S") ss))
    `(let ((,fun ,f)
           ,@(mapcar #'(lambda (acc) (list acc nil)) accumulators))
       (mapc #'(lambda ,ss
                 (multiple-value-bind ,vs (funcall ,fun ,@ss)
                   ,@(mapcar #'(lambda (accumulator v) (list 'push v accumulator))
                             accumulators vs)))
             ,@srcs)
       (let ,(mapcar #'(lambda (var accumulator) (list var (list 'nreverse accumulator)))
                     vars accumulators)
         ,@body))))


;;; ------------------------------------------------------------------------------------------------------
;;; VALUE ASSERTS

(defconstant *value-asserts* t)

; Assert that (test value) returns non-nil.  Return value.
(defmacro assert-value (value test &rest format-and-parameters)
  (if *value-asserts*
    (let ((v (gensym "VALUE")))
      `(let ((,v ,value))
         (unless (,test ,v)
           ,(if format-and-parameters
              `(error ,@format-and-parameters)
              `(error "~S doesn't satisfy ~S" ',value ',test)))
         ,v))
    value))


; Assert that value is non-nil.  Return value.
(defmacro assert-non-null (value &rest format-and-parameters)
  `(assert-value ,value identity .
                 ,(or format-and-parameters
                      `("~S is null" ',value))))


; Assert that value is non-nil.  Return nil.
; Do not evaluate value in nondebug versions.
(defmacro assert-true (value &rest format-and-parameters)
  (if *value-asserts*
    `(unless ,value
       ,(if format-and-parameters
          `(error ,@format-and-parameters)
          `(error "~S is false" ',value)))
    nil))


; Assert that expr returns n values.  Return those values.
(defmacro assert-n-values (n expr)
  (if *value-asserts*
    (let ((v (gensym "VALUES")))
      `(let ((,v (multiple-value-list ,expr)))
         (unless (= (length ,v) ,n)
           (error "~S returns ~D values instead of ~D" ',expr (length ,v) ',n))
         (values-list ,v)))
    expr))

; Assert that expr returns one value.  Return that value.
(defmacro assert-one-value (expr)
  `(assert-n-values 1 ,expr))

; Assert that expr returns two values.  Return those values.
(defmacro assert-two-values (expr)
  `(assert-n-values 2 ,expr))

; Assert that expr returns three values.  Return those values.
(defmacro assert-three-values (expr)
  `(assert-n-values 3 ,expr))


;;; ------------------------------------------------------------------------------------------------------
;;; STRUCTURED TYPES

(defconstant *type-asserts* t)

(defun tuple? (value structured-types)
  (if (endp structured-types)
    (null value)
    (and (consp value)
         (structured-type? (car value) (first structured-types))
         (tuple? (cdr value) (rest structured-types)))))

(defun list-of? (value structured-type)
  (or
   (null value)
   (and (consp value)
        (structured-type? (car value) structured-type)
        (list-of? (cdr value) structured-type))))


; Return true if value has the given structured-type.
; A structured-type can be a Common Lisp type or one of the forms below:
;
;   (cons t1 t2)  is the type of pairs whose car has structured-type t1 and
;                 cdr has structured-type t2.
;
;   (tuple t1 t2 ... tn) is the type of n-element lists whose first element
;                 has structured-type t1, second element has structured-type t2, ...,
;                 and last element has structured-type tn.
;
;   (list t)      is the type of lists all of whose elements have structured-type t.
;
(defun structured-type? (value structured-type)
  (cond
   ((consp structured-type)
    (case (first structured-type)
      (cons (and (consp value)
                 (structured-type? (car value) (second structured-type))
                 (structured-type? (cdr value) (third structured-type))))
      (tuple (tuple? value (rest structured-type)))
      (list (list-of? value (second structured-type)))
      (t (typep value structured-type))))
   ((null structured-type) nil)
   (t (typep value structured-type))))


; Ensure that value has type given by typespec
; (which should not be quoted).  Return the value.
(defmacro assert-type (value structured-type)
  (if *type-asserts*
    (let ((v (gensym "VALUE")))
      `(let ((,v ,value))
         (unless (structured-type? ,v ',structured-type)
           (error "~S should have type ~S" ,v ',structured-type))
         ,v))
    value))

(deftype bool () '(member nil t))


;;; ------------------------------------------------------------------------------------------------------
;;; GENERAL UTILITIES


; f must be either a function, a symbol, or a list of the form (setf <symbol>).
; If f is a function or has a function binding, return that function; otherwise return nil.
(defun callable (f)
  (cond
   ((functionp f) f)
   ((fboundp f) (fdefinition f))
   (t nil)))


; Return the first character of symbol's name or nil if s's name has zero length.
(defun first-symbol-char (symbol)
  (let ((name (symbol-name symbol)))
    (when (> (length name) 0)
      (char name 0))))


(defconstant *get2-nonce* (if (boundp '*get2-nonce*) (symbol-value '*get2-nonce*) (gensym)))

; Perform a get except that return two values:
;   The value returned from the get or nil if the property is not present
;   t if the property is present or nil if not.
(defun get2 (symbol property)
  (let ((value (get symbol property *get2-nonce*)))
    (if (eq value *get2-nonce*)
      (values nil nil)
      (values value t))))


; Return a list of all the keys in the hash table.
(defun hash-table-keys (hash-table)
  (let ((keys nil))
    (maphash #'(lambda (key value)
                 (declare (ignore value))
                 (push key keys))
             hash-table)
    keys))


; Return a list of all the keys in the hash table sorted by their string representations.
(defun sorted-hash-table-keys (hash-table)
  (with-standard-io-syntax
    (let ((*print-readably* nil)
          (*print-escape* nil))
      (sort (hash-table-keys hash-table) #'string< :key #'write-to-string))))


; Given an association list ((key1 . data1) (key2 . data2) ... (keyn datan)),
; produce another association list whose keys are sets of the keys of the original list,
; where the data elements of each such set are equal according to the given test function.
; The keys within each set are listed in the same order as in the original list.
; Set X comes before set Y if X contains a key earlier in the original list than any
; key in Y.
(defun collect-equivalences (alist &key (test #'eql))
  (if (endp alist)
    nil
    (let* ((element (car alist))
           (key (car element))
           (data (cdr element))
           (rest (cdr alist)))
      (if (rassoc data rest :test test)
        (let ((filtered-rest nil)
              (additional-keys nil))
          (dolist (elt rest)
            (if (funcall test data (cdr elt))
              (push (car elt) additional-keys)
              (push elt filtered-rest)))
          (acons (cons key (nreverse additional-keys)) data
                 (collect-equivalences (nreverse filtered-rest) :test test)))
        (acons (list key) data (collect-equivalences rest :test test))))))


;;; ------------------------------------------------------------------------------------------------------
;;; BITMAPS

; Treating integer m as a bitmap, call f on the number of each bit set in m.
(defun bitmap-each-bit (f m)
  (assert-true (>= m 0))
  (dotimes (i (integer-length m))
    (when (logbitp i m)
      (funcall f i))))

  
; Treating integer m as a bitmap, return a sorted list of disjoint, nonadjacent ranges
; of bits set in m.  Each range is a pair (x . y) and indicates that bits numbered x through
; y, inclusive, are set in m.  If m is negative, the last range will be a pair (x . :infinity).
(defun bitmap-to-ranges (m)
  (labels
    ((bitmap-to-ranges-sub (m ranges)
       (if (zerop m)
         ranges
         (let* ((hi (integer-length m))
                (m (- m (ash 1 hi)))
                (lo (integer-length m))
                (m (+ m (ash 1 lo))))
           (bitmap-to-ranges-sub m (acons lo (1- hi) ranges))))))
    (if (minusp m)
      (let* ((lo (integer-length m))
             (m (+ m (ash 1 lo))))
        (bitmap-to-ranges-sub m (list (cons lo :infinity))))
      (bitmap-to-ranges-sub m nil))))


; Same as bitmap-to-ranges but abbreviate pairs (x . x) by x.
(defun bitmap-to-abbreviated-ranges (m)
  (mapcar #'(lambda (range)
              (if (eql (car range) (cdr range))
                (car range)
                range))
          (bitmap-to-ranges m)))


;;; ------------------------------------------------------------------------------------------------------
;;; PACKAGES

; Call f on each external symbol defined in the package.
(defun each-package-external-symbol (package f)
  (with-package-iterator (iter package :external)
    (loop
      (multiple-value-bind (present symbol) (iter)
        (unless present
          (return))
        (funcall f symbol)))))


; Return a list of all external symbols defined in the package.
(defun package-external-symbols (package)
  (with-package-iterator (iter package :external)
    (let ((list nil))
      (loop
        (multiple-value-bind (present symbol) (iter)
          (unless present
            (return))
          (push symbol list)))
      list)))


; Return a sorted list of all external symbols defined in the package.
(defun sorted-package-external-symbols (package)
  (sort (package-external-symbols package) #'string<))


; Call f on each internal symbol defined in the package.
(defun each-package-internal-symbol (package f)
  (with-package-iterator (iter package :internal)
    (loop
      (multiple-value-bind (present symbol) (iter)
        (unless present
          (return))
        (funcall f symbol)))))


; Return a list of all internal symbols defined in the package.
(defun package-internal-symbols (package)
  (with-package-iterator (iter package :internal)
    (let ((list nil))
      (loop
        (multiple-value-bind (present symbol) (iter)
          (unless present
            (return))
          (push symbol list)))
      list)))


; Return a sorted list of all internal symbols defined in the package.
(defun sorted-package-internal-symbols (package)
  (sort (package-internal-symbols package) #'string<))


;;; ------------------------------------------------------------------------------------------------------
;;; SETS

(defstruct (set (:constructor allocate-set (elts-hash)))
  (elts-hash nil :type hash-table :read-only t))


; Make and return a new set.
(defun make-set (&optional (test #'eql))
  (allocate-set (make-hash-table :test test)))


; Add values to the set, modifying the set in place.
; Return the set.
(defun set-add (set &rest values)
  (let ((elements (set-elts-hash set)))
    (dolist (value values)
      (setf (gethash value elements) t)))
  set)


; Return true if element is a member of the set.
(defun set-member (set element)
  (gethash element (set-elts-hash set)))


; Return the set as a list.
(defun set-elements (set)
  (let ((elements nil))
    (maphash #'(lambda (key value)
                 (declare (ignore value))
                 (push key elements))
             (set-elts-hash set))
    elements))


; Print the set
(defmethod print-object ((set set) stream)
  (if *print-readably*
    (call-next-method)
    (format stream "~<{~;~@{~W ~:_~}~;}~:>" (set-elements set))))


;;; ------------------------------------------------------------------------------------------------------
;;; DEPTH-FIRST SEARCH

; Return a depth-first-ordered list of the nodes in a directed graph.
; The graph may contain cycles, so a general depth-first search is used.
; start is the start node.
; successors is a function that takes a node and returns a list of that
;   node's successors.
; test is a function that takes two nodes and returns true if they are
;   the same node.  test should be either #'eq, #'eql, or #'equal
;   because it is used as a test function in a hash table.
(defun depth-first-search (test successors start)
  (let ((visited-nodes (make-set test))
        (dfs-list nil))
    (labels
      ((visit (node)
         (set-add visited-nodes node)
         (dolist (successor (funcall successors node))
           (unless (set-member visited-nodes successor)
             (visit successor)))
         (push node dfs-list)))
      (visit start)
      dfs-list)))
