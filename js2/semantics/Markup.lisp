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
;;; Common RTF and HTML writing utilities
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(defvar *trace-logical-blocks* nil)     ;Emit logical blocks to *trace-output* while processing
(defvar *show-logical-blocks* nil)      ;Emit logical block boundaries as hidden rtf text

(defvar *markup-logical-line-width* 90) ;Approximate maximum number of characters to display on a single logical line
(defvar *average-space-width* 2/3)      ;Width of a space as a percentage of average character width when calculating logical line widths

(defvar *compact-breaks* t)             ;If true, all hard breaks are replaced by spaces and there is no indentation

(defvar *external-link-base* nil)       ;URL prefix for referring to a page with external links or nil if none


;;; ------------------------------------------------------------------------------------------------------
;;; LINK TABLES

; Return a table for recording defined, referenced, and external links.
; External links include a # character; locally defined and referenced ones do not.
(declaim (inline make-link-table))
(defun make-link-table ()
  (make-hash-table :test #'equal))


; The concatenation of link-prefix and link-name is the name of a link.  Mark the link defined.
; Return the full name if links are allowed and this is the first definition of that name.
; If duplicate is false, don't allow multiple definitions of the same link name.
(defun record-link-definition (links link-prefix link-name duplicate)
  (assert-type link-prefix string)
  (assert-type link-name string)
  (and links
       (let ((name (concatenate 'string link-prefix link-name)))
         (cond
          ((not (eq (gethash name links) :defined))
           (setf (gethash name links) :defined)
           name)
          (duplicate nil)
          (t (warn "Duplicate link definition ~S" name)
             name)))))


; The concatenation of link-prefix and link-name is the name of a link.  Mark the link referenced.
; If external is true, the link refers to the page given by *external-link-base*; if *external-link-base*
; is null and external is true, no link gets made.
; Return the full href if links are allowed or nil if not.
(defun record-link-reference (links link-prefix link-name external)
  (assert-type link-prefix string)
  (assert-type link-name string)
  (and links
       (if external
         (and *external-link-base*
              (let ((href (concatenate 'string *external-link-base* "#" link-prefix link-name)))
                (setf (gethash href links) :external)
                href))
         (let ((name (concatenate 'string link-prefix link-name)))
           (unless (eq (gethash name links) :defined)
             (setf (gethash name links) :referenced))
           (concatenate 'string "#" name)))))


; Warn about all referenced but not defined links.
(defun warn-missing-links (links)
  (when links
    (let ((missing-links nil)
          (external-links nil))
      (maphash #'(lambda (name link-state)
                   (case link-state
                     (:referenced (push name missing-links))
                     (:external (push name external-links))))
               links)
      (setq missing-links (sort missing-links #'string<))
      (setq external-links (sort external-links #'string<))
      (when missing-links
        (warn "The following links have been referenced but not defined: ~S" missing-links))
      (when external-links
        (format *error-output* "External links:~%~{  ~A~%~}" external-links)))))


; Return a list of all prefixes of the form "A-" where "A" is any character for all defined links.
(defun links-defined-prefixes (links)
  (let ((prefix-letters nil))
    (maphash #'(lambda (name link-state)
                 (when (and (eq link-state :defined)
                            (>= (length name) 2)
                            (eql (char name 1) #\-))
                   (pushnew (char name 0) prefix-letters)))
             links)
    (mapcar #'(lambda (letter) (coerce (list letter #\-) 'string))
            prefix-letters)))


;;; ------------------------------------------------------------------------------------------------------
;;; MARKUP ENVIRONMENTS


(defstruct (markup-env (:constructor allocate-markup-env (macros widths)))
  (macros nil :type hash-table :read-only t)     ;Hash table of keyword -> expansion list
  (widths nil :type hash-table :read-only t)     ;Hash table of keyword -> estimated width of macro expansion;
;                                                ;  zero-width entries can be omitted; multiline entries have t for a width.
  (links nil :type (or null hash-table)))        ;Hash table of string -> either :referenced or :defined;
;                                                ;  nil if links not supported


; Make a markup-env.  If links is true, allow links.
(defun make-markup-env (links)
  (let ((markup-env (allocate-markup-env (make-hash-table :test #'eq) (make-hash-table :test #'eq))))
    (when links
      (setf (markup-env-links markup-env) (make-link-table)))
    markup-env))


; Recursively expand all keywords in markup-tree, producing a freshly consed expansion tree.
; Allow keywords in the permitted-keywords list to be present in the output without generating an error.
(defun markup-env-expand (markup-env markup-tree permitted-keywords)
  (mapcan
   #'(lambda (markup-element)
       (cond
        ((keywordp markup-element)
         (let ((expansion (gethash markup-element (markup-env-macros markup-env) *get2-nonce*)))
           (if (eq expansion *get2-nonce*)
             (if (member markup-element permitted-keywords :test #'eq)
               (list markup-element)
               (error "Unknown markup macro ~S" markup-element))
             (markup-env-expand markup-env expansion permitted-keywords))))
        ((listp markup-element)
         (list (markup-env-expand markup-env markup-element permitted-keywords)))
        (t (list markup-element))))
   markup-tree))


(defun markup-env-define (markup-env keyword expansion &optional width)
  (assert-type keyword keyword)
  (assert-type expansion (list t))
  (assert-type width (or null integer (eql t)))
  (when (gethash keyword (markup-env-macros markup-env))
    (warn "Redefining markup macro ~S" keyword))
  (setf (gethash keyword (markup-env-macros markup-env)) expansion)
  (if width
    (setf (gethash keyword (markup-env-widths markup-env)) width)
    (remhash keyword (markup-env-widths markup-env))))


(defun markup-env-append (markup-env keyword expansion)
  (assert-type keyword keyword)
  (assert-type expansion (list t))
  (setf (gethash keyword (markup-env-macros markup-env))
        (append (gethash keyword (markup-env-macros markup-env)) expansion)))


(defun markup-env-define-alist (markup-env keywords-and-expansions)
  (dolist (keyword-and-expansion keywords-and-expansions)
    (let ((keyword (car keyword-and-expansion))
          (expansion (cdr keyword-and-expansion)))
      (cond
       ((not (consp keyword))
        (markup-env-define markup-env keyword expansion))
       ((eq (first keyword) '+)
        (markup-env-append markup-env (second keyword) expansion))
       (t (markup-env-define markup-env (first keyword) expansion (second keyword)))))))


;;; ------------------------------------------------------------------------------------------------------
;;; LOGICAL POSITIONS

(defstruct logical-position
  (n-hard-breaks 0 :type integer)             ;Number of :new-line's in the current paragraph or logical block
  (position 0 :type integer)                  ;Current character position.  If n-hard-breaks is zero, only includes characters written into this logical block
  ;                                           ;  plus the minimal position from the enclosing block.  If n-hard-breaks is nonzero, includes indent and characters
  ;                                           ;  written since the last hard break.
  (surplus 0 :type integer)                   ;Value to subtract from position if soft breaks were hard breaks in this logical block
  (n-soft-breaks nil :type (or null integer)) ;Number of soft-breaks in the current paragraph or nil if not inside a depict-logical-block
  (indent 0 :type (or null integer)))         ;Indent for next line


; Return the value the position would have if soft breaks became hard breaks in this logical block.
(declaim (inline logical-position-minimal-position))
(defun logical-position-minimal-position (logical-position)
  (- (logical-position-position logical-position) (logical-position-surplus logical-position)))


; Advance the logical position by width characters.  If width is t,
; advance to the next line.
(defun logical-position-advance (logical-position width)
  (if (eq width t)
    (progn
      (incf (logical-position-n-hard-breaks logical-position))
      (setf (logical-position-position logical-position) 0)
      (setf (logical-position-surplus logical-position) 0))
    (incf (logical-position-position logical-position) width)))


(defstruct (soft-break (:constructor make-soft-break (width groups)))
  (width 0 :type integer)      ;Number of spaces by which to replace this soft break if it doesn't turn into a hard break; t if unconditional
  (groups nil :type list))     ;List of groups to be added to the new line if a line break happens here


; Destructively replace any soft-break that appears in a car position in the tree with
; the spliced result of calling f on that soft-break.  f should return a non-null list that can
; be nconc'd.
(defun substitute-soft-breaks (tree f)
  (do ((subtree tree next-subtree)
       (next-subtree (cdr tree) (cdr next-subtree)))
      ((endp subtree))
    (let ((item (car subtree)))
      (cond
       ((soft-break-p item)
        (let* ((splice (assert-non-null (funcall f item)))
               (splice-rest (cdr splice)))
          (setf (car subtree) (car splice))
          (setf (cdr subtree) (nconc splice-rest next-subtree))))
       ((consp item) (substitute-soft-breaks item f)))))
  tree)


; Destructively replace any soft-break that appears in a car position in the tree
; with width spaces, where width is the soft-break's width.
(defun remove-soft-breaks (tree)
  (substitute-soft-breaks
   tree
   #'(lambda (soft-break)
       (list (make-string (soft-break-width soft-break) :initial-element #\space :element-type #-mcl 'character #+mcl 'base-character)))))


; Return a freshly consed markup list for a hard line break followed by indent spaces.
(defun hard-break-markup (indent)
  (cond
   (*compact-breaks* (list :space))
   ((zerop indent) (list :new-line))
   (t (list :new-line (make-string indent :initial-element #\space :element-type #-mcl 'character #+mcl 'base-character)))))



; Destructively replace any soft-break that appears in a car position in the tree
; with a line break followed by indent spaces.
; Note that if the markup-stream's tail was pointing to a soft break, it may now not point
; to the last cons cell of the tree and should be adjusted.
(defun expand-soft-breaks (tree indent)
  (substitute-soft-breaks
   tree
   #'(lambda (soft-break)
       (nconc (hard-break-markup indent)
              (copy-list (soft-break-groups soft-break))))))


;;; ------------------------------------------------------------------------------------------------------
;;; MARKUP STREAMS

(defstruct (markup-stream (:copier nil) (:predicate markup-stream?))
  (env nil :type markup-env :read-only t)
  (level nil :type integer)           ;0 for emitting top-level group; 1 for emitting sections; 2 for emitting paragraphs; 3 for emitting paragraph contents
  (head nil :type list)               ;Pointer to a dummy cons-cell whose cdr is the output markup list.
  ;                                   ; A markup-stream may destructively modify any sublists of head that contain a soft-break.
  (tail nil :type list)               ;Last cons cell of the output list; new cells are added in place to this cell's cdr; nil after markup-stream is closed.
  (pretail nil :type list)            ;Tail's predecessor if tail's car is a block that can be inlined at the end of the output list; nil otherwise.
  (logical-line-width 0 :type integer);Logical line width for the current paragraph
  (division-length nil)               ;Number of characters in the current division block; t if more than one line; nil if not emitting division block.
  (logical-position nil :type logical-position)) ;Information about the current logical lines or nil if not emitting paragraph contents

;                                                ;RTF                     ;HTML
(defconstant *markup-stream-top-level* 0)        ;Top-level group         ;Top level
(defconstant *markup-stream-section-level* 1)    ;Sections                ;(not used)
(defconstant *markup-stream-paragraph-level* 2)  ;Paragraphs              ;Block tags
(defconstant *markup-stream-content-level* 3)    ;Paragraph contents      ;Inline tags


; Add additional-length to this markup-stream's division-length.  additional-length may be t, which sets
; division-length to t.  division-length is left alone if it was nil.
(defun increment-division-length (markup-stream additional-length)
  (cond
   ((not (numberp (markup-stream-division-length markup-stream))))
   ((eq additional-length t) (setf (markup-stream-division-length markup-stream) t))
   (t (incf (markup-stream-division-length markup-stream) additional-length))))


; Return the markup accumulated in the markup-stream.
; The markup-stream is closed after this function is called.
(defun markup-stream-unexpanded-output (markup-stream)
  (when (markup-stream-pretail markup-stream)
    ;Inline the last block at the end of the markup-stream.
    (setf (cdr (markup-stream-pretail markup-stream)) (car (markup-stream-tail markup-stream)))
    (setf (markup-stream-pretail markup-stream) nil))
  (setf (markup-stream-tail markup-stream) nil) ;Close the stream.
  (cdr (assert-non-null (markup-stream-head markup-stream))))


; Return the markup accumulated in the markup-stream after expanding all of its macros.
; The markup-stream is closed after this function is called.
(defgeneric markup-stream-output (markup-stream))


; Append one item to the end of the markup-stream.
(defun markup-stream-append1 (markup-stream item)
  (setf (markup-stream-pretail markup-stream) nil)
  (let ((item-cons (list item)))
    (setf (cdr (markup-stream-tail markup-stream)) item-cons)
    (setf (markup-stream-tail markup-stream) item-cons)))


; Append a list of items to the end of the markup-stream.
; The list becomes part of the markup-stream's structure and will be mutated by subsequent operations
; on the markup-stream.
(defun markup-stream-append-list (markup-stream items)
  (when items
    (setf (markup-stream-pretail markup-stream) nil)
    (setf (cdr (markup-stream-tail markup-stream)) items)
    (setf (markup-stream-tail markup-stream) (last items))))


; Return the approximate width of the markup item; return t if it is a line break.
(defun markup-width (markup-stream item)
  (cond
   ((stringp item) (round (- (length item) (* (count #\space item) (- 1 *average-space-width*)))))
   ((keywordp item) (gethash item (markup-env-widths (markup-stream-env markup-stream)) 0))
   ((and item (symbolp item)) 0)
   (t (error "Bad item in markup-width" item))))


; Return the approximate width of the markup item; return t if it is a line break.
; Also allow markup groups as long as they do not contain line breaks.
(defgeneric markup-group-width (markup-stream item))


; Append zero or more markup items to the end of the markup-stream.
; The items must be either keywords, symbols, or strings.
(defun depict (markup-stream &rest markup-list)
  (assert-true (>= (markup-stream-level markup-stream) *markup-stream-content-level*))
  (dolist (markup markup-list)
    (markup-stream-append1 markup-stream markup)
    (logical-position-advance (markup-stream-logical-position markup-stream) (markup-width markup-stream markup))))


; Same as depict except that the items may be groups as well.
(defun depict-group (markup-stream &rest markup-list)
  (assert-true (>= (markup-stream-level markup-stream) *markup-stream-content-level*))
  (dolist (markup markup-list)
    (markup-stream-append1 markup-stream markup)
    (logical-position-advance (markup-stream-logical-position markup-stream) (markup-group-width markup-stream markup))))


; If markup-item-or-list is a list, emit its contents via depict.
; If markup-item-or-list is not a list, emit it via depict.
(defun depict-item-or-list (markup-stream markup-item-or-list)
  (if (listp markup-item-or-list)
    (apply #'depict markup-stream markup-item-or-list)
    (depict markup-stream markup-item-or-list)))


; If markup-item-or-list is a list, emit its contents via depict-group.
; If markup-item-or-list is not a list, emit it via depict.
(defun depict-item-or-group-list (markup-stream markup-item-or-list)
  (if (listp markup-item-or-list)
    (apply #'depict-group markup-stream markup-item-or-list)
    (depict markup-stream markup-item-or-list)))


; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraphs.  Execute body with markup-stream bound to a markup-stream
; to which the body can emit contents.  If non-null, the given division-style is applied to all
; paragraphs emitted by body.
; If flatten is true, do not emit the style if it is already in effect from a surrounding division
; or if its contents are empty.
; Return the result value of body.
(defmacro depict-division-style ((markup-stream division-style &optional flatten) &body body)
  `(depict-division-style-f ,markup-stream ,division-style ,flatten
                            #'(lambda (,markup-stream) ,@body)))

(defgeneric depict-division-style-f (markup-stream division-style flatten emitter))


; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraphs.  Execute body with markup-stream bound to a markup-stream
; to which the body can emit divisions and paragraphs.  If everything the body emits
; could fit on one line, collapse out any sub-divisions whose styles are a member of the
; division-styles list.  The result should be zero or more paragraphs all having
; paragraph styles that are members of the paragraph-styles list; coalesce them into a single
; paragraph with style paragraph-style.
; Return the result value of body.
(defmacro depict-division-block ((markup-stream paragraph-style paragraph-styles division-styles) &body body)
  `(depict-division-block-f ,markup-stream ,paragraph-style ,paragraph-styles ,division-styles
                           #'(lambda (,markup-stream) ,@body)))

(defgeneric depict-division-block-f (markup-stream paragraph-style paragraph-styles division-styles emitter))


; Prevent any enclosing depict-division-block from collapsing its contents.
(defun depict-division-break (markup-stream)
  (assert-true (<= (markup-stream-level markup-stream) *markup-stream-paragraph-level*))
  (when (numberp (markup-stream-division-length markup-stream))
    (setf (markup-stream-division-length markup-stream) t)))



; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraphs.  Emit a paragraph with the given paragraph-style (which
; must be a symbol) whose contents are emitted by body.  When executing body,
; markup-stream is bound to a markup-stream to which body should emit the paragraph's contents.
; Return the result value of body.
(defmacro depict-paragraph ((markup-stream paragraph-style) &body body)
  `(depict-paragraph-f ,markup-stream ,paragraph-style
                       #'(lambda (,markup-stream) ,@body)))

(defgeneric depict-paragraph-f (markup-stream paragraph-style emitter))


; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraph contents.  Execute body with markup-stream bound to a markup-stream
; to which the body can emit contents.  If non-null, the given char-style is applied to all such
; contents emitted by body.
; Return the result value of body.
(defmacro depict-char-style ((markup-stream char-style) &body body)
  `(depict-char-style-f ,markup-stream ,char-style
                        #'(lambda (,markup-stream) ,@body)))

(defgeneric depict-char-style-f (markup-stream char-style emitter))


; Ensure that the given style is not currently in effect in the markup-stream.
; RTF streams don't currently keep track of styles, so this function does nothing for RTF streams.
(defgeneric ensure-no-enclosing-style (markup-stream style))


; Return a value that captures the current sequence of enclosing division styles.
(defgeneric save-division-style (markup-stream))

; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraphs.  Execute body with markup-stream bound to a markup-stream
; to which the body can emit contents.  The given saved-division-style is applied to all
; paragraphs emitted by body (in the HTML emitter only; RTF has no division styles).
; saved-division-style should have been obtained from a past call to save-division-style.
; If flatten is true, do not emit the style if it is already in effect from a surrounding block
; or if its contents are empty.
; Return the result value of body.
(defmacro with-saved-division-style ((markup-stream saved-division-style &optional flatten) &body body)
  `(with-saved-division-style-f ,markup-stream ,saved-division-style ,flatten
     #'(lambda (,markup-stream) ,@body)))

(defgeneric with-saved-division-style-f (markup-stream saved-division-style flatten emitter))


; Depict an anchor.  The concatenation of link-prefix and link-name must be a string
; suitable for an anchor name.
; If duplicate is true, allow duplicate calls for the same link-name, in which case only
; the first one takes effect.
(defgeneric depict-anchor (markup-stream link-prefix link-name duplicate))


; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraph contents.  Execute body with markup-stream bound to a markup-stream
; to which the body can emit contents.  The given link name is the destination of a local
; link for which body is the contents.  The concatenation of link-prefix and link-name
; must be a string suitable for an anchor name.
; Return the result value of body.
(defmacro depict-link-reference ((markup-stream link-prefix link-name external) &body body)
  `(depict-link-reference-f ,markup-stream ,link-prefix ,link-name ,external
                            #'(lambda (,markup-stream) ,@body)))

(defgeneric depict-link-reference-f (markup-stream link-prefix link-name external emitter))


; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraph contents.  Execute body with markup-stream bound to a markup-stream
; to which the body can emit contents.  Depending on link, do one of the following:
;   :reference   Emit a reference to the link with the given body of the reference;
;   :external    Emit an external reference to the link with the given body of the reference;
;   :definition  Emit the link as an anchor, followed by the body;
;   nil          Emit the body only.
; If duplicate is true, allow duplicate anchors, in which case only the first one takes effect.
; Return the result value of body.
(defmacro depict-link ((markup-stream link link-prefix link-name duplicate) &body body)
  `(depict-link-f ,markup-stream ,link ,link-prefix ,link-name ,duplicate
                  #'(lambda (,markup-stream) ,@body)))

(defun depict-link-f (markup-stream link link-prefix link-name duplicate emitter)
  (ecase link
    (:reference (depict-link-reference-f markup-stream link-prefix link-name nil emitter))
    (:external (depict-link-reference-f markup-stream link-prefix link-name t emitter))
    (:definition
      (depict-anchor markup-stream link-prefix link-name duplicate)
      (funcall emitter markup-stream))
    ((nil) (funcall emitter markup-stream))))


(defun depict-logical-block-f (markup-stream indent emitter)
  (assert-true (>= (markup-stream-level markup-stream) *markup-stream-content-level*))
  (if indent
    (let* ((logical-position (markup-stream-logical-position markup-stream))
           (cumulative-indent (+ (logical-position-indent logical-position) indent))
           (minimal-position (logical-position-minimal-position logical-position))
           (inner-logical-position (make-logical-position :position minimal-position
                                                          :n-soft-breaks 0
                                                          :indent cumulative-indent))
           (old-tail (markup-stream-tail markup-stream)))
      (setf (markup-stream-logical-position markup-stream) inner-logical-position)
      (when *show-logical-blocks*
        (markup-stream-append1 markup-stream (list :invisible (format nil "<~D" indent))))
      (prog1
        (funcall emitter markup-stream)
        (when *show-logical-blocks*
          (markup-stream-append1 markup-stream '(:invisible ">")))
        (assert-true (eq (markup-stream-logical-position markup-stream) inner-logical-position))
        (let* ((tree (cdr old-tail))
               (inner-position (logical-position-position inner-logical-position))
               (inner-count (- inner-position minimal-position))
               (inner-n-hard-breaks (logical-position-n-hard-breaks inner-logical-position))
               (inner-n-soft-breaks (logical-position-n-soft-breaks inner-logical-position)))
          (when *trace-logical-blocks*
            (format *trace-output* "Block ~:W:~%position ~D,  count ~D,  n-hard-breaks ~D,  n-soft-breaks ~D~%~%"
                    tree inner-position inner-count inner-n-hard-breaks inner-n-soft-breaks))
          (cond
           ((zerop inner-n-soft-breaks)
            (assert-true (zerop (logical-position-surplus inner-logical-position)))
            (if (zerop inner-n-hard-breaks)
              (incf (logical-position-position logical-position) inner-count)
              (progn
                (incf (logical-position-n-hard-breaks logical-position) inner-n-hard-breaks)
                (setf (logical-position-position logical-position) inner-position)
                (setf (logical-position-surplus logical-position) 0))))
           ((and (zerop inner-n-hard-breaks) (<= inner-position (markup-stream-logical-line-width markup-stream)))
            (assert-true tree)
            (remove-soft-breaks tree)
            (incf (logical-position-position logical-position) inner-count))
           (t
            (let ((tail (markup-stream-tail markup-stream)))
              (assert-true (and tree (consp tail) (null (cdr tail))))
              (expand-soft-breaks tree cumulative-indent)
              (setf (markup-stream-tail markup-stream) (assert-non-null (last tail))))
            (incf (logical-position-n-hard-breaks logical-position) (+ inner-n-hard-breaks inner-n-soft-breaks))
            (setf (logical-position-position logical-position) (logical-position-minimal-position inner-logical-position))
            (setf (logical-position-surplus logical-position) 0))))
        (setf (markup-stream-logical-position markup-stream) logical-position)))
    (funcall emitter markup-stream)))


; markup-stream must be a variable that names a markup-stream that is currently
; accepting paragraph contents.  Execute body with markup-stream bound to a markup-stream
; to which the body can emit contents.  body can call depict-break, which will either
; all expand to the widths given to the depict-break calls or all expand to line breaks
; followed by indents to the current indent level plus the given indent.
; If indent is nil, don't create the logical block and just evaluate body.
; Return the result value of body.
(defmacro depict-logical-block ((markup-stream indent) &body body)
  `(depict-logical-block-f ,markup-stream ,indent
                           #'(lambda (,markup-stream) ,@body)))


; Emit a conditional line break.  If the line break is not needed, emit width spaces instead.
; If width is t or omitted, the line break is unconditional.
; If width is nil, do nothing.
; If the line break is needed, the new line is indented to the current indent level and groups,
; if provided, are added to the beginning of the new line.  The width of these groups is currently not taken
; into account.
; Must be called from the dynamic scope of a depict-logical-block.
(defun depict-break (markup-stream &optional (width t) &rest groups)
  (assert-true (>= (markup-stream-level markup-stream) *markup-stream-content-level*))
  (when width
    (let* ((logical-position (markup-stream-logical-position markup-stream))
           (indent (logical-position-indent logical-position)))
      (if (eq width t)
        (progn
          (depict-item-or-list markup-stream (hard-break-markup indent))
          (dolist (item groups)
            (markup-stream-append1 markup-stream item)))
        (progn
          (incf (logical-position-n-soft-breaks logical-position))
          (incf (logical-position-position logical-position) width)
          (let ((surplus (- (logical-position-position logical-position) (round (* indent *average-space-width*)))))
            (when (< surplus 0)
              (setq surplus 0))
            (setf (logical-position-surplus logical-position) surplus))
          (when *show-logical-blocks*
            (markup-stream-append1 markup-stream '(:invisible :bullet)))
          (markup-stream-append1 markup-stream (make-soft-break width groups)))))))


; Call emitter to emit each element of the given list onto the markup-stream.
; emitter takes two arguments -- the markup-stream and the element of list to be emitted.
; Emit prefix before the list and suffix after the list.  If prefix-break is supplied, call
; depict-break with it as the argument after the prefix.
; If indent is non-nil, enclose the list elements in a logical block with the given indent.
; Emit separator between any two emitted elements.  If break is supplied, call
; depict-break with it as the argument after each separator.
; If the list is empty, emit empty unless it is :error, in which case signal an error.
;
; prefix, suffix, separator, and empty should be lists of markup elements appropriate for depict.
; If any of these lists has only one element that is not itself a list, then that list can be
; abbreviated to just that element (as in depict-item-or-list).
;
(defun depict-list (markup-stream emitter list &key indent prefix prefix-break suffix separator break (empty :error))
  (assert-true (or indent (not (or prefix-break break))))
  (labels
    ((emit-element (markup-stream list)
       (funcall emitter markup-stream (first list))
       (let ((rest (rest list)))
         (when rest
           (depict-item-or-list markup-stream separator)
           (depict-break markup-stream break)
           (emit-element markup-stream rest)))))
    
    (depict-item-or-list markup-stream prefix)
    (cond
     (list
      (depict-logical-block (markup-stream indent)
        (depict-break markup-stream prefix-break)
        (emit-element markup-stream list)))
     ((eq empty :error) (error "Non-empty list required"))
     (t (depict-item-or-list markup-stream empty)))
    (depict-item-or-list markup-stream suffix)))


;;; ------------------------------------------------------------------------------------------------------
;;; MARKUP FOR CHARACTERS AND STRINGS

(defparameter *character-names*
  '((#x00 . "NUL")
    (#x08 . "BS")
    (#x09 . "TAB")
    (#x0A . "LF")
    (#x0B . "VT")
    (#x0C . "FF")
    (#x0D . "CR")
    (#x20 . "SP")))

; Emit markup for the given character.  The character is emitted without any formatting if it is a
; printable character and not a member of the escape-list list of characters.  Otherwise the
; character is emitted with :character-literal-control formatting.
; The markup-stream should already be set to :character-literal formatting.
(defun depict-character (markup-stream char &optional (escape-list '(#\space)))
  (let ((code (char-code char)))
    (if (and (>= code 32) (< code 127) (not (member char escape-list)))
      (depict markup-stream (string char))
      (depict-char-style (markup-stream :character-literal-control)
        (let ((name (or (cdr (assoc code *character-names*))
                        (format nil "u~4,'0X" code))))
          (depict markup-stream :left-angle-quote name :right-angle-quote))))))


; Emit markup for the given string, enclosing it in curly double quotes.
; The markup-stream should be set to normal formatting.
(defun depict-string (markup-stream string)
  (depict markup-stream :left-double-quote)
  (unless (equal string "")
    (depict-char-style (markup-stream :character-literal)
      (dotimes (i (length string))
        (depict-character markup-stream (char string i) nil))))
  (depict markup-stream :right-double-quote))


;;; ------------------------------------------------------------------------------------------------------
;;; IDENTIFIER ABBREVIATIONS

; Return a symbol with the same package as the given symbol but whose name omits everything
; after the first underscore, if any, in the given symbol's name.  The returned symbol is eq
; to the given symbol if its name contains no underscores.
(defun symbol-to-abbreviation (symbol)
  (let* ((name (symbol-name symbol))
         (pos (position #\_ name)))
    (if pos
      (intern (subseq name 0 pos) (symbol-package symbol))
      symbol)))


; A caching version of symbol-to-abbreviation.
(defun symbol-abbreviation (symbol)
  (or (get symbol :abbreviation)
      (setf (get symbol :abbreviation) (symbol-to-abbreviation symbol))))


;;; ------------------------------------------------------------------------------------------------------
;;; MARKUP FOR IDENTIFIERS

; Return string converted from dash-separated-uppercase-words to mixed case,
; with the first character capitalized if capitalize is true.
; The string should contain only letters, dashes, and numbers.
(defun string-to-mixed-case (string &optional capitalize)
  (let* ((length (length string))
         (dst-string (make-array length :element-type #-mcl 'character #+mcl 'base-character :fill-pointer 0)))
    (dotimes (i length)
      (let ((char (char string i)))
        (if (eql char #\-)
          (if capitalize
            (error "Double capitalize")
            (setq capitalize t))
          (progn
            (cond
             ((upper-case-p char)
              (if capitalize
                (setq capitalize nil)
                (setq char (char-downcase char))))
             ((digit-char-p char))
             ((member char '(#\$ #\_)))
             (t (error "Bad string-to-mixed-case character ~A" char)))
            (vector-push char dst-string)))))
    dst-string))


; Return a string containing the symbol's name in mixed case with the first letter capitalized.
(defun symbol-upper-mixed-case-name (symbol)
  (or (get symbol :upper-mixed-case-name)
      (setf (get symbol :upper-mixed-case-name) (string-to-mixed-case (symbol-name symbol) t))))


; Return a string containing the symbol's name in mixed case with the first letter in lower case.
(defun symbol-lower-mixed-case-name (symbol)
  (or (get symbol :lower-mixed-case-name)
      (setf (get symbol :lower-mixed-case-name) (string-to-mixed-case (symbol-name symbol)))))


;;; ------------------------------------------------------------------------------------------------------
;;; MISCELLANEOUS MARKUP


; Append a space to the end of the markup-stream.
(defun depict-space (markup-stream)
  (depict markup-stream " "))


; Emit markup for the given integer, displaying it in decimal.
(defun depict-integer (markup-stream i)
  (when (minusp i)
    (depict markup-stream :minus)
    (setq i (- i)))
  (depict markup-stream (format nil "~D" i)))


(defmacro styled-text-depictor (symbol)
  `(get ,symbol 'styled-text-depictor))


; Emit markup for the given <text>, which should be a list of:
;   <string>                display as is
;   <keyword>               display as is
;   (<symbol> . <args>)     if <symbol>'s styled-text-depictor property is present, call it giving it <args>
;                             as arguments; otherwise treat this case as the following:
;   (<style> . <text>)      display <text> with the given <style> keyword
;   <character>             display using depict-character
(defun depict-styled-text (markup-stream text)
  (dolist (item text)
    (cond
     ((or (stringp item) (keywordp item))
      (depict markup-stream item))
     ((consp item)
      (let* ((first (first item))
             (rest (rest item))
             (depictor (styled-text-depictor first)))
        (cond
         (depictor (apply depictor markup-stream rest))
         ((keywordp first) (depict-char-style (markup-stream first)
                             (depict-styled-text markup-stream rest)))
         (t (error "Bad depict-styled-text style: ~S" first)))))
     ((characterp item)
      (depict-character markup-stream item))
     (t (error "Bad depict-styled-text item: ~S" item)))))
