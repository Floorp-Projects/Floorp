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
;;; Handy lisp utilities
;;;
;;; Waldemar Horwat (waldemar@acm.org)
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
; var1 through varn-1 may be nil, in which case it is not bound.
(defmacro list*-bind ((var1 &rest vars) expr &body body)
  (labels
    ((gen-let*-bindings (var1 vars expr)
       (if vars
         (if var1
           (let ((expr-var (gensym "REST")))
             (list*
              (list expr-var expr)
              (list var1 (list 'car expr-var))
              (gen-let*-bindings (car vars) (cdr vars) (list 'cdr expr-var))))
           (gen-let*-bindings (car vars) (cdr vars) (list 'cdr expr)))
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

(eval-when (:compile-toplevel :load-toplevel :execute)
  (defconstant *value-asserts* t))

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

; Assert that expr returns four values.  Return those values.
(defmacro assert-four-values (expr)
  `(assert-n-values 4 ,expr))


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
      (sort (hash-table-keys hash-table) #'string<
            :key #'(lambda (item)
                     (if (symbolp item)
                       (or (get item :sort-key)
                           (symbol-name item))
                       (write-to-string item)))))))


; Return an association list of all the entries in the hash table.
(defun hash-table-entries (hash-table)
  (let ((entries nil))
    (maphash #'(lambda (key value)
                 (push (cons key value) entries))
             hash-table)
    entries))


; Return true if the two hash tables are equal, using the given equality test for testing their elements.
(defun hash-table-= (hash-table1 hash-table2 &key (test #'eql))
  (and (= (hash-table-count hash-table1) (hash-table-count hash-table2))
       (progn
         (maphash
          #'(lambda (key1 value1)
              (multiple-value-bind (value2 present2) (gethash key1 hash-table2)
                (unless (and present2 (funcall test value1 value2))
                  (return-from hash-table-= nil))))
          hash-table1)
         t)))


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


; Return true if item is a member of the tree of conses.
(defun tree-member (item tree &key (test #'eql))
  (cond
   ((funcall test item tree))
   ((consp tree)
    (or (tree-member item (car tree) :test test)
        (tree-member item (cdr tree) :test test)))
   (t nil)))


;;; ------------------------------------------------------------------------------------------------------
;;; TIME


(defun time-to-string (time)
  (multiple-value-bind (second minute hour day month year weekday) (decode-universal-time time)
    (declare (ignore second minute hour))
    (format nil "~A, ~A ~D, ~D"
            (nth weekday '("Monday" "Tuesday" "Wednesday" "Thursday" "Friday" "Saturday" "Sunday"))
            (nth (1- month) '("January" "February" "March" "April" "May" "June" "July" "August" "September" "October" "November" "December"))
            day
            year)))


(defun time-to-short-string (time)
  (multiple-value-bind (second minute hour day month year weekday) (decode-universal-time time)
    (declare (ignore second minute hour weekday))
    (format nil "~D-~A-~D"
            day
            (nth (1- month) '("Jan" "Feb" "Mar" "Apr" "May" "Jun" "Jul" "Aug" "Sep" "Oct" "Nov" "Dec"))
            year)))


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
;;; INTSETS

;;; An intset is a finite set of integers, represented as an ordered list of ranges.
;;; Each range is a cons (low . high), both low and high being inclusive.  Ranges must
;;; be nonoverlapping, and adjacent ranges must be consolidated.

(defconstant *empty-intset* nil)

; Return true if the intset is valid.
(defun valid-intset? (intset)
  (and (structured-type? intset '(list (cons integer integer)))
       (or (null intset)
           (let ((prev (- (caar intset) 2)))
             (dolist (range intset t)
               (let ((low (car range))
                     (high (cdr range)))
                 (unless (and (< prev (1- low)) (<= low high))
                   (return nil))
                 (setq prev high)))))))


; Return an intset that is the union of the given intset and the intset
; containg the given values.
(defun intset-add-value (intset &rest values)
  (labels
    ((add-value (intset value)
       (if (endp intset)
         (list (cons value value))
         (let* ((first-range (first intset))
                (rest (rest intset))
                (first-low (car first-range))
                (first-high (cdr first-range)))
           (cond
            ((> value first-high)
             (cond
              ((/= value (1+ first-high)) (cons first-range (add-value rest value)))
              ((or (endp rest) (/= (caar rest) (1+ value))) (acons first-low value rest))
              (t (acons first-low (cdar rest) (rest rest)))))
            ((< value first-low)
             (if (/= value (1- first-low))
               (acons value value intset)
               (acons value first-high rest)))
            (t intset))))))
    
    (dolist (value values)
      (assert-true (integerp value))
      (add-value intset value))))


; Return an intset that is the union of the given intset and the intset
; containg all integers between low and high, inclusive.  low <= high+1 is required.
(defun intset-add-range (intset low high)
  (labels
    ((add-range (intset low high)
       (if (endp intset)
         (list (cons low high))
         (let* ((first-range (first intset))
                (rest (rest intset))
                (first-low (car first-range))
                (first-high (cdr first-range)))
           (cond
            ((> low (1+ first-high))
             (cons first-range (add-range rest low high)))
            ((< high (1- first-low))
             (acons low high intset))
            ((<= high first-high)
             (if (>= low first-low)
               intset
               (acons low first-high rest)))
            (t (add-range rest (min low first-low) high)))))))
    
    (assert-true (and (integerp low) (integerp high) (<= low (1+ high))))
    (if (= low (1+ high))
      intset
      (add-range intset low high))))


; Return an intset constructed from a list of ranges.  Each range has two expressions,
; low and high.  high can be null to indicate a one-element range.
(defun intset-from-ranges (&rest ranges)
  (if (endp ranges)
    *empty-intset*
    (progn
      (assert-true (cdr ranges))
      (intset-add-range (apply #'intset-from-ranges (cddr ranges))
                        (first ranges)
                        (or (second ranges) (first ranges))))))



; Return true if value is a member of the intset.
(defun intset-member? (value intset)
  (if (endp intset)
    nil
    (let ((first-range (first intset)))
      (if (> value (cdr first-range))
        (intset-member? value (rest intset))
        (>= value (car first-range))))))


; Return the union of the two intsets.
(defun intset-union (intset1 intset2)
  (cond
   ((endp intset1) intset2)
   ((endp intset2) intset1)
   (t (let* ((first-range1 (first intset1))
             (rest1 (rest intset1))
             (first-low1 (car first-range1))
             (first-high1 (cdr first-range1))
             (first-range2 (first intset2))
             (rest2 (rest intset2))
             (first-low2 (car first-range2))
             (first-high2 (cdr first-range2)))
        (cond
         ((< first-high1 (1- first-low2))
          (cons first-range1 (intset-union rest1 intset2)))
         ((< first-high2 (1- first-low1))
          (cons first-range2 (intset-union intset1 rest2)))
         (t (intset-union (intset-add-range intset1 first-low2 first-high2) rest2)))))))


; Return the intersection of the two intsets.
(defun intset-intersection (intset1 intset2)
  (if (or (endp intset1) (endp intset2))
    nil
    (let* ((first-range1 (first intset1))
           (rest1 (rest intset1))
           (first-low1 (car first-range1))
           (first-high1 (cdr first-range1))
           (first-range2 (first intset2))
           (rest2 (rest intset2))
           (first-low2 (car first-range2))
           (first-high2 (cdr first-range2))
           (low (max first-low1 first-low2)))
      (cond
       ((< first-high1 first-high2)
        (if (<= low first-high1)
          (acons low first-high1 (intset-intersection rest1 intset2))
          (intset-intersection rest1 intset2)))
       ((> first-high1 first-high2)
        (if (<= low first-high2)
          (acons low first-high2 (intset-intersection intset1 rest2))
          (intset-intersection intset1 rest2)))
       (t (acons low first-high1 (intset-intersection rest1 rest2)))))))


; Return the the intset containing the elements of intset1 that are not in intset2.
(defun intset-difference (intset1 intset2)
  (cond
   ((endp intset1) nil)
   ((endp intset2) intset1)
   (t (let* ((first-range1 (first intset1))
             (rest1 (rest intset1))
             (first-low1 (car first-range1))
             (first-high1 (cdr first-range1))
             (first-range2 (first intset2))
             (rest2 (rest intset2))
             (first-low2 (car first-range2))
             (first-high2 (cdr first-range2)))
        (cond
         ((< first-high1 first-low2)
          (cons first-range1 (intset-difference rest1 intset2)))
         ((> first-low1 first-high2)
          (intset-difference intset1 rest2))
         ((< first-low1 first-low2)
          (acons first-low1 (1- first-low2) (intset-difference (acons first-low2 first-high1 rest1) intset2)))
         ((> first-high1 first-high2)
          (intset-difference (acons (1+ first-high2) first-high1 rest1) rest2))
         (t (intset-difference rest1 intset2)))))))


; Return true if the two intsets are equal.
(declaim (inline intset=))
(defun intset= (intset1 intset2)
  (equal intset1 intset2))

(defconstant intset=-name 'equal)


; Return true if the intset is empty.
(declaim (inline intset-empty))
(defun intset-empty (intset)
  (endp intset))


; Return the number of elements in the intset.
(defun intset-length (intset)
  (if (endp intset)
    0
    (+ 1 (- (cdar intset) (caar intset))
       (intset-length (rest intset)))))


; Return the lowest element of the intset or nil if the intset is empty.
(declaim (inline intset-min))
(defun intset-min (intset)
  (caar intset))


; Return the highest element of the intset or nil if the intset is empty.
(defun intset-max (intset)
  (cdar (last intset)))


;;; ------------------------------------------------------------------------------------------------------
;;; PARTIAL ORDERS

(defstruct partial-order
  (next-number 0 :type integer))  ;Bit number to use for next element


(defstruct (partial-order-element (:constructor make-partial-order-element (partial-order number predecessor-bitmap))
                                  (:copier nil)
                                  (:predicate partial-order-element?))
  (partial-order nil :type partial-order)  ;Partial order to which this element belongs
  (number nil :type integer)               ;Bit number of this element
  (predecessor-bitmap nil :type integer))  ;Bitmap of elements less than or equal to this one in the partial order


; Construct a new unique element in the partial order that is greater than the
; given predecessors.  Return that element.
(defun partial-order-add-element (partial-order &rest predecessors)
  (let* ((number (partial-order-next-number partial-order))
         (predecessor-bitmap (ash 1 number)))
    (dolist (predecessor predecessors)
      (assert-true (eq (partial-order-element-partial-order predecessor) partial-order))
      (setq predecessor-bitmap (logior predecessor-bitmap (partial-order-element-predecessor-bitmap predecessor))))
    (incf (partial-order-next-number partial-order))
    (make-partial-order-element partial-order number predecessor-bitmap)))


(defmacro def-partial-order-element (partial-order name &rest predecessors)
  `(defparameter ,name (partial-order-add-element ,partial-order ,@predecessors)))


; Return true if element1 is greater than or equal to element2 in this partial order.
(defun partial-order->= (element1 element2)
  (assert-true (eq (partial-order-element-partial-order element1) (partial-order-element-partial-order element2)))
  (logbitp (partial-order-element-number element2) (partial-order-element-predecessor-bitmap element1)))


; Return true if element1 is less than element2 in this partial order.
(declaim (inline partial-order-<))
(defun partial-order-< (element1 element2)
  (not (partial-order->= element1 element2)))


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
  (let ((visited-nodes (make-hash-table :test test))
        (dfs-list nil))
    (labels
      ((visit (node)
         (setf (gethash node visited-nodes) t)
         (dolist (successor (funcall successors node))
           (unless (gethash successor visited-nodes)
             (visit successor)))
         (push node dfs-list)))
      (visit start)
      dfs-list)))
