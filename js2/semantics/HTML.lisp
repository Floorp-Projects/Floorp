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
;;; HTML output generator
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


;;; ------------------------------------------------------------------------------------------------------
;;; ELEMENTS

(defstruct (html-element (:constructor make-html-element (name self-closing indent
                                                               newlines-before newlines-begin newlines-end newlines-after))
                         (:predicate html-element?))
  (name nil :type symbol :read-only t)               ;Name of the tag
  (self-closing nil :type bool :read-only t)         ;True if the closing tag should be omitted
  (indent nil :type integer :read-only t)            ;Number of spaces by which to indent this tag's contents in HTML source
  (newlines-before nil :type integer :read-only t)   ;Number of HTML source newlines preceding the opening tag
  (newlines-begin nil :type integer :read-only t)    ;Number of HTML source newlines immediately following the opening tag
  (newlines-end nil :type integer :read-only t)      ;Number of HTML source newlines immediately preceding the closing tag
  (newlines-after nil :type integer :read-only t))   ;Number of HTML source newlines following the closing tag


; Define symbol to refer to the given html-element.
(defun define-html (symbol newlines-before newlines-begin newlines-end newlines-after &key self-closing (indent 0))
  (setf (get symbol 'html-element) (make-html-element symbol self-closing indent
                                                      newlines-before newlines-begin newlines-end newlines-after)))


;;; ------------------------------------------------------------------------------------------------------
;;; ELEMENT DEFINITIONS

(define-html 'a 0 0 0 0)
(define-html 'b 0 0 0 0)
(define-html 'blockquote 1 0 0 1 :indent 2)
(define-html 'body 1 1 1 1)
(define-html 'br 0 0 0 1 :self-closing t)
(define-html 'code 0 0 0 0)
(define-html 'dd 1 0 0 1 :indent 2)
(define-html 'del 0 0 0 0)
(define-html 'div 1 0 0 1 :indent 2)
(define-html 'dl 1 0 0 2 :indent 2)
(define-html 'dt 1 0 0 1 :indent 2)
(define-html 'em 0 0 0 0)
(define-html 'h1 2 0 0 2 :indent 2)
(define-html 'h2 2 0 0 2 :indent 2)
(define-html 'h3 2 0 0 2 :indent 2)
(define-html 'h4 2 0 0 2 :indent 2)
(define-html 'h5 2 0 0 2 :indent 2)
(define-html 'h6 2 0 0 2 :indent 2)
(define-html 'head 1 1 1 2)
(define-html 'hr 1 0 0 1 :self-closing t)
(define-html 'html 0 1 1 1)
(define-html 'i 0 0 0 0)
(define-html 'li 1 0 0 1 :indent 2)
(define-html 'link 1 0 0 1 :self-closing t)
(define-html 'ol 1 1 1 2 :indent 2)
(define-html 'p 2 0 0 2)
(define-html 'script 0 0 0 0)
(define-html 'span 0 0 0 0)
(define-html 'strong 0 0 0 0)
(define-html 'sub 0 0 0 0)
(define-html 'sup 0 0 0 0)
(define-html 'table 1 1 1 1)
(define-html 'td 1 0 0 1 :indent 2)
(define-html 'th 1 0 0 1 :indent 2)
(define-html 'title 1 0 0 1)
(define-html 'tr 1 0 0 1 :indent 2)
(define-html 'u 0 0 0 0)
(define-html 'ul 1 1 1 2 :indent 2)
(define-html 'var 0 0 0 0)


;;; ------------------------------------------------------------------------------------------------------
;;; ATTRIBUTES

;;; The following element attributes require their values to always be in quotes.
(dolist (attribute '(alt href name))
  (setf (get attribute 'quoted-attribute) t))


;;; ------------------------------------------------------------------------------------------------------
;;; ENTITIES

(defvar *html-entities-list*
  '((#\& . "amp")
    (#\" . "quot")
    (#\< . "lt")
    (#\> . "gt")
    (nbsp . "nbsp")))

(defvar *html-entities-hash* (make-hash-table))

(dolist (entity-binding *html-entities-list*)
  (setf (gethash (first entity-binding) *html-entities-hash*) (rest entity-binding)))


; Return a freshly consed list of <html-source> that represent the characters in the string except that
; '&', '<', and '>' are replaced by their entities and spaces are replaced by the entity
; given by the space parameter (which should be either 'space or 'nbsp).
(defun escape-html-characters (string space)
  (let ((html-sources nil))
    (labels
      ((escape-remainder (start)
         (let ((i (position-if #'(lambda (char) (member char '(#\& #\< #\> #\space))) string :start start)))
           (if i
             (let ((char (char string i)))
               (unless (= i start)
                 (push (subseq string start i) html-sources))
               (push (if (eql char #\space) space char) html-sources)
               (escape-remainder (1+ i)))
             (push (if (zerop start) string (subseq string start)) html-sources)))))
      (unless (zerop (length string))
        (escape-remainder 0))
      (nreverse html-sources))))


; Escape all content strings in the html-source, while interpreting :nowrap, :wrap, :space, and :none pseudo-tags.
; Return a freshly consed list of html-sources.
(defun escape-html-source (html-source space)
  (cond
   ((stringp html-source)
    (escape-html-characters html-source space))
   ((eq html-source :space) (list 'space))
   ((or (characterp html-source) (symbolp html-source) (integerp html-source))
    (list html-source))
   ((consp html-source)
    (let ((tag (first html-source))
          (contents (rest html-source)))
      (case tag
        (:none (mapcan #'(lambda (html-source) (escape-html-source html-source space)) contents))
        (:nowrap (mapcan #'(lambda (html-source) (escape-html-source html-source 'nbsp)) contents))
        (:wrap (mapcan #'(lambda (html-source) (escape-html-source html-source 'space)) contents))
        (t (list (cons tag
                       (mapcan #'(lambda (html-source) (escape-html-source html-source space)) contents)))))))
   (t (error "Bad html-source: ~S" html-source))))


; Escape all content strings in the html-source, while interpreting :nowrap, :wrap, :space, and :none pseudo-tags.
(defun escape-html (html-source)
  (let ((results (escape-html-source html-source 'space)))
    (assert-true (= (length results) 1))
    (first results)))


;;; ------------------------------------------------------------------------------------------------------
;;; HTML WRITER

;; <html-source> has one of the following formats:
;;   <string>                                                  ;String to be printed literally
;;   <symbol>                                                  ;Named entity
;;   <integer>                                                 ;Numbered entity
;;   space                                                     ;Space or newline
;;   :space                                                    ;Breaking space, regardless of enclosing :nowrap tags
;;   (<tag> <html-source> ... <html-source>)                   ;Tag and its contents
;;   ((:nest <tag> ... <tag>) <html-source> ... <html-source>) ;Equivalent to (<tag> (... (<tag> <html-source> ... <html-source>)))
;;
;; <tag> has one of the following formats:
;;   <symbol>                                                  ;Tag with no attributes
;;   (<symbol> <attribute> ... <attribute>)                    ;Tag with attributes
;;   :nowrap                                                   ;Pseudo-tag indicating that spaces in contents should be non-breaking
;;   :wrap                                                     ;Pseudo-tag indicating that spaces in contents should be breaking
;;   :none                                                     ;Pseudo-tag indicating no tag -- the contents should be inlined
;;
;; <attribute> has one of the following formats:
;;   (<symbol> <string>)                                       ;Attribute name and value
;;   (<symbol>)                                                ;Attribute name with omitted value


(defparameter *html-right-margin* 120)
(defparameter *allow-line-breaks-in-tags* nil) ;Allow line breaks in tags between attributes?

(defvar *current-html-pos*)            ;Number of characters written to the current line of the stream; nil if *current-html-newlines* is nonzero
(defvar *current-html-indent*)         ;Indent to use for emit-html-newlines-and-indent calls
(defvar *current-html-pending*)        ;String following a space or newline pending to be printed on the current line or nil if none
(defvar *current-html-pending-indent*) ;Indent to use if *current-html-pending* is placed on a new line
(defvar *current-html-newlines*)       ;Number of consecutive newlines just written to the stream; zero if last character wasn't a newline


; Flush *current-html-pending* onto the stream.
(defun flush-current-html-pending (stream)
  (when *current-html-pending*
    (unless (zerop (length *current-html-pending*))
      (write-char #\space stream)
      (write-string *current-html-pending* stream)
      (incf *current-html-pos* (1+ (length *current-html-pending*))))
    (setq *current-html-pending* nil)))


; Emit n-newlines onto the stream and indent the next line by *current-html-indent* spaces.
(defun emit-html-newlines-and-indent (stream n-newlines)
  (decf n-newlines *current-html-newlines*)
  (when (plusp n-newlines)
    (flush-current-html-pending stream)
    (dotimes (i n-newlines)
      (write-char #\newline stream))
    (incf *current-html-newlines* n-newlines)
    (setq *current-html-pos* nil)))


; Write the string to the stream, observing *current-html-pending* and *current-html-pos*.
(defun write-html-string (stream html-string)
  (unless (zerop (length html-string))
    (unless *current-html-pos*
      (setq *current-html-newlines* 0)
      (write-string (make-string *current-html-indent* :initial-element #\space) stream)
      (setq *current-html-pos* *current-html-indent*))
    (if *current-html-pending*
      (progn
        (setq *current-html-pending* (if (zerop (length *current-html-pending*))
                                       html-string
                                       (concatenate 'string *current-html-pending* html-string)))
        (when (>= (+ *current-html-pos* (length *current-html-pending*)) *html-right-margin*)
          (write-char #\newline stream)
          (write-string (make-string *current-html-pending-indent* :initial-element #\space) stream)
          (write-string *current-html-pending* stream)
          (setq *current-html-pos* (+ *current-html-pending-indent* (length *current-html-pending*)))
          (setq *current-html-pending* nil)))
      (progn
        (write-string html-string stream)
        (incf *current-html-pos* (length html-string))))))


; Return true if the value string contains a character that would require an attribute to be quoted.
; For convenience, this returns true if value contains a period, even though strictly speaking periods do
; not force quoting.
(defun attribute-value-needs-quotes (value)
  (dotimes (i (length value) nil)
    (let ((ch (char value i)))
      (unless (or (char<= #\0 ch #\9) (char<= #\A ch #\Z) (char<= #\a ch #\z) (char= ch #\-))
        (return t)))))


; Emit the html tag with the given tag-symbol (name), attributes, and contents.
(defun write-html-tag (stream tag-symbol attributes contents)
  (let ((element (assert-non-null (get tag-symbol 'html-element))))
    (emit-html-newlines-and-indent stream (html-element-newlines-before element))
    (write-html-string stream (format nil "<~A" (html-element-name element)))
    (let ((*current-html-indent* (+ *current-html-indent* (html-element-indent element))))
      (dolist (attribute attributes)
        (let ((name (first attribute))
              (value (second attribute)))
          (write-html-source stream (if *allow-line-breaks-in-tags* 'space #\space))
          (write-html-string stream (string-downcase (symbol-name name)))
          (when value
            (write-html-string
             stream
             (format nil
                     (if (or (attribute-value-needs-quotes value)
                             (get name 'quoted-attribute))
                       "=\"~A\""
                       "=~A")
                     value)))))
      (write-html-string stream ">")
      (emit-html-newlines-and-indent stream (html-element-newlines-begin element))
      (dolist (html-source contents)
        (write-html-source stream html-source)))
    (unless (html-element-self-closing element)
      (emit-html-newlines-and-indent stream (html-element-newlines-end element))
      (write-html-string stream (format nil "</~A>" (html-element-name element))))
    (emit-html-newlines-and-indent stream (html-element-newlines-after element))))


; Write html-source to the character stream.
(defun write-html-source (stream html-source)
  (cond
   ((stringp html-source)
    (write-html-string stream html-source))
   ((eq html-source 'space)
    (when (zerop *current-html-newlines*)
      (flush-current-html-pending stream)
      (setq *current-html-pending* "")
      (setq *current-html-pending-indent* *current-html-indent*)))
   ((or (characterp html-source) (symbolp html-source))
    (let ((entity-name (gethash html-source *html-entities-hash*)))
      (cond
       (entity-name
        (write-html-string stream (format nil "&~A;" entity-name)))
       ((characterp html-source)
        (write-html-string stream (string html-source)))
       (t (error "Bad html-source ~S" html-source)))))
   ((integerp html-source)
    (assert-true (and (>= html-source 0) (< html-source 65536)))
    (write-html-string stream (format nil "&#~D;" html-source)))
   ((consp html-source)
    (let ((tag (first html-source))
          (contents (rest html-source)))
      (if (consp tag)
        (write-html-tag stream (first tag) (rest tag) contents)
        (write-html-tag stream tag nil contents))))
   (t (error "Bad html-source: ~S" html-source))))


; Write the top-level html-source to the character stream.
(defun write-html (html-source &optional (stream t))
  (with-standard-io-syntax
    (let ((*print-readably* nil)
          (*print-escape* nil)
          (*print-case* :upcase)
          (*current-html-pos* nil)
          (*current-html-indent* 0)
          (*current-html-pending* nil)
          (*current-html-pending-indent* nil)
          (*current-html-newlines* 9999))
      (write-string "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" \"http://www.w3.org/TR/REC-html40/loose.dtd\">" stream)
      (write-char #\newline stream)
      (write-html-source stream (escape-html html-source)))))


; Write html to the text file with the given name (relative to the
; local directory).
(defun write-html-to-local-file (filename html)
  (with-open-file (stream (filename-to-semantic-engine-pathname filename)
                          :direction :output
                          :if-exists :supersede
                          #+mcl :mac-file-creator #+mcl "MOSS")
    (write-html html stream)))


; Expand the :nest constructs inside html-source.
(defun unnest-html-source (html-source)
  (labels
    ((unnest-tags (tags contents)
       (assert-true tags)
       (cons (first tags)
             (if (endp (rest tags))
               contents
               (list (unnest-tags (rest tags) contents))))))
    (if (consp html-source)
      (let ((tag (first html-source))
            (contents (rest html-source)))
        (if (and (consp tag) (eq (first tag) :nest))
          (unnest-html-source (unnest-tags (rest tag) contents))
          (cons tag (mapcar #'unnest-html-source contents))))
      html-source)))


; Coalesce an A element immediately containing or contained in a SPAN element into one if their attributes
; are disjoint.  Also coalesce SUB and SUP elements immediately containing SPAN elements into one.
(defun coalesce-elements (html-source)
  (if (consp html-source)
    (let ((tag (first html-source))
          (contents (mapcar #'coalesce-elements (rest html-source))))
      (cond
       ((and (consp tag)
             (member (first tag) '(a span))
             contents
             (null (cdr contents))
             (consp (car contents))
             (let ((tag2 (caar contents)))
               (and (consp tag2)
                    (member (first tag2) '(a span))
                    (not (eq tag tag2))
                    (null (intersection (rest tag) (rest tag2) :key #'car)))))
        (cons
         (cons 'a
               (if (eq (first tag) 'a)
                 (append (rest tag) (rest (caar contents)))
                 (append (rest (caar contents)) (rest tag))))
         (cdar contents)))
       ((and (member tag '(sub sup))
             contents
             (null (cdr contents))
             (consp (car contents))
             (consp (caar contents))
             (eq (caaar contents) 'span))
        (cons
         (cons tag (rest (caar contents)))
         (cdar contents)))
       (t (cons tag contents))))
    html-source))


;;; ------------------------------------------------------------------------------------------------------
;;; HTML MAPPINGS

(defparameter *html-definitions*
  '(((:new-line t) (br))
    
    ;Misc.
    ((:nbsp 1) nbsp)
    (:tab2 nbsp nbsp)
    (:tab3 nbsp nbsp nbsp)
    (:nbhy "-")             ;Non-breaking hyphen
    
    ;Symbols (-10 suffix means 10-point, etc.)
    ((:bullet 1) (:script "document.write(U_bull)"))                    ;#x2022
    ((:not-equal 1) (:script "document.write(U_ne)"))                   ;#x2260
    ((:less-or-equal 1) (:script "document.write(U_le)"))               ;#x2264
    ((:greater-or-equal 1) (:script "document.write(U_ge)"))            ;#x2265
    ((:infinity 1) (:script "document.write(U_infin)"))                 ;#x221E
    ((:minus 1) #x2013)
    ((:m-dash 2) #x2014)
    ((:left-single-quote 1) #x2018)
    ((:right-single-quote 1) #x2019)
    ((:apostrophe 1) #x2019)
    ((:left-double-quote 1) #x201C)
    ((:right-double-quote 1) #x201D)
    ((:left-angle-quote 1) #x00AB)
    ((:right-angle-quote 1) #x00BB)
    ((:for-all-10 1) (:script "document.write(U_forall)"))              ;#x2200
    ((:exists-10 1) (:script "document.write(U_exist)"))                ;#x2203
    ((:bottom-10 1) (:script "document.write(U_perp)"))                 ;#x22A5
    ((:assign-10 1) (:script "document.write(U_larr)"))                 ;#x2190
    ((:up-arrow-10 1) (:script "document.write(U_uarr)"))               ;#x2191
    ((:function-arrow-10 2) (:script "document.write(U_rarr)"))         ;#x2192
    ((:cartesian-product-10 2) (:script "document.write(U_times)"))     ;#x00D7
    ((:identical-10 2) (:script "document.write(U_equiv)"))             ;#x2261
    ((:circle-plus-10 2) (:script "document.write(U_oplus)"))           ;#x2295
    ((:empty-10 2) (:script "document.write(U_empty)"))                 ;#x2205
    ((:intersection-10 1) (:script "document.write(U_cap)"))            ;#x2229
    ((:union-10 1) (:script "document.write(U_cup)"))                   ;#x222A
    ((:subset-10 2) (:script "document.write(U_sub)"))                  ;#x2282
    ((:subset-eq-10 2) (:script "document.write(U_sube)"))              ;#x2286
    ((:member-10 2) (:script "document.write(U_isin)"))                 ;#x2208
    ((:not-member-10 2) (:script "document.write(U_notin)"))            ;#x2209
    ((:label-assign-10 2) (:script "document.write(U_lArr)"))           ;#x21D0
    ((:derives-10 2) (:script "document.write(U_rArr)"))                ;#x21D2
    ((:left-triangle-bracket-10 1) (:script "document.write(U_lang)"))  ;#x2329
    ((:left-ceiling-10 1) (:script "document.write(U_lceil)"))          ;#x2308
    ((:left-floor-10 1) (:script "document.write(U_lfloor)"))           ;#x230A
    ((:right-triangle-bracket-10 1) (:script "document.write(U_rang)")) ;#x232A
    ((:right-ceiling-10 1) (:script "document.write(U_rceil)"))         ;#x2309
    ((:right-floor-10 1) (:script "document.write(U_rfloor)"))          ;#x230B
    
    ((:alpha 1) (:script "document.write(U_alpha)"))
    ((:beta 1) (:script "document.write(U_beta)"))
    ((:chi 1) (:script "document.write(U_chi)"))
    ((:delta 1) (:script "document.write(U_delta)"))
    ((:epsilon 1) (:script "document.write(U_epsilon)"))
    ((:phi 1) (:script "document.write(U_phi)"))
    ((:gamma 1) (:script "document.write(U_gamma)"))
    ((:eta 1) (:script "document.write(U_eta)"))
    ((:iota 1) (:script "document.write(U_iota)"))
    ((:kappa 1) (:script "document.write(U_kappa)"))
    ((:lambda 1) (:script "document.write(U_lambda)"))
    ((:mu 1) (:script "document.write(U_mu)"))
    ((:nu 1) (:script "document.write(U_nu)"))
    ((:omicron 1) (:script "document.write(U_omicron)"))
    ((:pi 1) (:script "document.write(U_pi)"))
    ((:theta 1) (:script "document.write(U_theta)"))
    ((:rho 1) (:script "document.write(U_rho)"))
    ((:sigma 1) (:script "document.write(U_sigma)"))
    ((:tau 1) (:script "document.write(U_tau)"))
    ((:upsilon 1) (:script "document.write(U_upsilon)"))
    ((:omega 1) (:script "document.write(U_omega)"))
    ((:xi 1) (:script "document.write(U_xi)"))
    ((:psi 1) (:script "document.write(U_psi)"))
    ((:zeta 1) (:script "document.write(U_zeta)"))
    
    ;Division styles
    (:js2 (div (class "js2")))
    (:es4 (div (class "es4")))
    (:body-text p)
    (:heading1 h1)
    (:heading2 h2)
    (:heading3 h3)
    (:heading4 h4)
    (:heading5 h5)
    (:heading6 h6)
    (:grammar-header h4)
    (:grammar-rule (div (class "grammar-rule")))
    (:grammar-lhs (div (class "grammar-lhs")))
    (:grammar-lhs-last :grammar-lhs)
    (:grammar-rhs (div (class "grammar-rhs")))
    (:grammar-rhs-last :grammar-rhs)
    (:grammar-argument (:nest :nowrap (div (class "grammar-argument"))))
    (:semantic-comment (div (class "semantic-comment")))
    (:algorithm (div (class "algorithm")))
    (:algorithm-stmt (div (class "algorithm-stmt")))
    ((:level 4) (div (class "lvl")))
    ((:level-wide 4) (div (class "lvl-wide")))
    (:statement (div (class "stmt")))
    (:statement-last :statement)
    
    ;Inline Styles
    (:script (script (type "text/javascript")))
    (:symbol (span (class "symbol")))
    (:character-literal code)
    (:character-literal-control (span (class "control")))
    (:terminal (span (class "terminal")))
    (:terminal-keyword (code (class "terminal-keyword")))
    (:terminal-sub (span (class "terminal-sub")))
    (:nonterminal (span (class "nonterminal")))
    (:nonterminal-attribute (span (class "nonterminal-attribute")))
    (:nonterminal-argument (span (class "nonterminal-argument")))
    (:nonterminal-sub (span (class "nonterminal-sub")))
    (:semantic-keyword (span (class "semantic-keyword")))
    (:domain-name (span (class "domain-name")))
    (:field-name (span (class "field-name")))
    (:tag-name (span (class "tag-name")))
    (:global-variable (span (class "global-variable")))
    (:variable var)
    (:action-name (span (class "action-name")))
    (:text :wrap)
    
    ;Specials
    (:invisible del)
    ((:but-not 6) (b "except"))
    ((:begin-negative-lookahead 13) "[lookahead" :not-member-10 "{")
    ((:end-negative-lookahead 2) "}]")
    ((:line-break 12) "[line" :nbsp "break]")
    ((:no-line-break 15) "[no" :nbsp "line" :nbsp "break]")
    (:subscript sub)
    (:superscript sup)
    ((:action-begin 1) "[")
    ((:action-end 1) "]")
    ((:vector-begin 1) (b "["))
    ((:vector-end 1) (b "]"))
    ((:empty-vector 2) (b "[]"))
    ((:vector-construct 1) (b "|"))
    ((:vector-append 2) :circle-plus-10)
    ((:tuple-begin 1) (b :left-triangle-bracket-10))
    ((:tuple-end 1) (b :right-triangle-bracket-10))
    ((:record-begin 1) (b (:script "document.write(U_lang+U_lang)")))
    ((:record-end 1) (b (:script "document.write(U_rang+U_rang)")))
    ((:true 4) (:global-variable "true"))
    ((:false 5) (:global-variable "false"))
    ((:unique 6) (:semantic-keyword "unique"))
    ))


;;; ------------------------------------------------------------------------------------------------------
;;; HTML STREAMS

(defstruct (html-stream (:include markup-stream)
                        (:constructor allocate-html-stream (env head tail level logical-line-width division-length logical-position
                                                                enclosing-styles anchors))
                        (:copier nil)
                        (:predicate html-stream?))
  (enclosing-styles nil :type list :read-only t) ;A list of enclosing styles
  (anchors nil :type list :read-only t))  ;A mutable cons cell for accumulating anchors at the beginning of a paragraph
;                                         ;or nil if not inside a paragraph.


(defmethod print-object ((html-stream html-stream) stream)
  (print-unreadable-object (html-stream stream :identity t)
    (write-string "html-stream" stream)))


; Make a new, empty, open html-stream with the given definitions for its markup-env.
(defun make-html-stream (markup-env level logical-line-width division-length logical-position enclosing-styles anchors)
  (let ((head (list nil)))
    (allocate-html-stream markup-env head head level logical-line-width division-length logical-position enclosing-styles anchors)))


; Make a new, empty, open, top-level html-stream with the given definitions
; for its markup-env.  If links is true, allow links.
(defun make-top-level-html-stream (html-definitions links)
  (let ((head (list nil))
        (markup-env (make-markup-env links)))
    (markup-env-define-alist markup-env html-definitions)
    (allocate-html-stream markup-env head head *markup-stream-top-level* *markup-logical-line-width* nil nil nil nil)))


; Return the approximate width of the html item; return t if it is a line break.
; Also allow html tags as long as they do not contain line breaks.
(defmethod markup-group-width ((html-stream html-stream) item)
  (if (consp item)
    (reduce #'+ (rest item) :key #'(lambda (subitem) (markup-group-width html-stream subitem)))
    (markup-width html-stream item)))


; Create a top-level html-stream and call emitter to emit its contents.
; emitter takes one argument -- an html-stream to which it should emit paragraphs.
; Return the top-level html-stream.  If links is true, allow links.
(defun depict-html-top-level (title links emitter)
  (let ((html-stream (make-top-level-html-stream *html-definitions* links)))
    (markup-stream-append1 html-stream 'html)
    (depict-division-style (html-stream 'head)
      (depict-division-style (html-stream 'title)
        (markup-stream-append1 html-stream title))
      (markup-stream-append1 html-stream '((link (rel "stylesheet") (href "../styles.css"))))
      (markup-stream-append1 html-stream '((script (type "text/javascript") (language "JavaScript1.2") (src "../unicodeCompatibility.js")))))
    (depict-division-style (html-stream 'body)
      (funcall emitter html-stream))
    (warn-missing-links (markup-env-links (html-stream-env html-stream)))
    html-stream))


; Create a top-level html-stream and call emitter to emit its contents.
; emitter takes one argument -- an html-stream to which it should emit paragraphs.
; Write the resulting html to the text file with the given name (relative to the
; local directory).
; If links is true, allow links.  If external-link-base is also provided, emit links for
; predefined items and assume that they are located on the page specified by the
; external-link-base string.
(defun depict-html-to-local-file (filename title links emitter &key external-link-base)
  (let* ((*external-link-base* external-link-base)
         (top-html-stream (depict-html-top-level title links emitter)))
    (write-html-to-local-file filename (markup-stream-output top-html-stream)))
  filename)


; Return the markup accumulated in the markup-stream after expanding all of its macros.
; The markup-stream is closed after this function is called.
(defmethod markup-stream-output ((html-stream html-stream))
  (coalesce-elements
   (unnest-html-source
    (markup-env-expand (markup-stream-env html-stream) (markup-stream-unexpanded-output html-stream) '(:none :nowrap :wrap :space :nest)))))



(defmethod depict-division-style-f ((html-stream html-stream) division-style flatten emitter)
  (assert-true (<= (markup-stream-level html-stream) *markup-stream-paragraph-level*))
  (assert-true (symbolp division-style))
  (if (or (null division-style)
          (and flatten (member division-style (html-stream-enclosing-styles html-stream))))
    (funcall emitter html-stream)
    (let ((inner-html-stream (make-html-stream (markup-stream-env html-stream)
                                               *markup-stream-paragraph-level*
                                               (- (html-stream-logical-line-width html-stream) (markup-width html-stream division-style))
                                               0
                                               nil
                                               (cons division-style (html-stream-enclosing-styles html-stream))
                                               nil)))
      (markup-stream-append1 inner-html-stream division-style)
      (prog1
        (funcall emitter inner-html-stream)
        (let ((inner-output (markup-stream-unexpanded-output inner-html-stream)))
          (when (or (not flatten) (cdr inner-output))
            (markup-stream-append1 html-stream inner-output)
            (increment-division-length html-stream (html-stream-division-length inner-html-stream))))))))


; html is the output from an html-stream consisting of paragraphs and/or divisions.  Every
; division must be one of the given division-styles and may contain other such divisions and/or
; paragraphs.  All paragraphs must have paragraph styles that are members of the paragraph-styles list.
; Return html flattened to a single paragraph with the given paragraph-style with spaces inserted
; between the component paragraphs.  May destroy the original html list.
(defun flatten-division-block (html paragraph-style paragraph-styles division-styles)
  (labels ((flatten-item (html-item)
             (let ((tag (first html-item))
                   (contents (rest html-item)))
               (cond
                ((member tag paragraph-styles) (cons " " contents))
                ((member tag division-styles :test #'eq)
                 (mapcan #'flatten-item contents))
                (t (error "Unable to flatten ~S" html-item))))))
    (if html
      (list (cons paragraph-style (cdr (mapcan #'flatten-item html))))
      nil)))


(defmethod depict-division-block-f ((html-stream html-stream) paragraph-style paragraph-styles division-styles emitter)
  (assert-true (<= (markup-stream-level html-stream) *markup-stream-paragraph-level*))
  (assert-true (and paragraph-style (symbolp paragraph-style)))
  (let* ((logical-line-width (html-stream-logical-line-width html-stream))
         (inner-html-stream (make-html-stream (markup-stream-env html-stream)
                                              *markup-stream-paragraph-level*
                                              logical-line-width
                                              0
                                              nil
                                              (html-stream-enclosing-styles html-stream)
                                              nil)))
    (prog1
      (funcall emitter inner-html-stream)
      (let ((inner-output (markup-stream-unexpanded-output inner-html-stream))
            (inner-length (html-stream-division-length inner-html-stream)))
        (unless (eq inner-length t)
          (if (> inner-length logical-line-width)
            (setq inner-length t)
            (setq inner-output (flatten-division-block inner-output paragraph-style paragraph-styles division-styles))))
        (increment-division-length html-stream inner-length)
        (markup-stream-append-list html-stream inner-output)))))


(defmethod depict-paragraph-f ((html-stream html-stream) paragraph-style emitter)
  (assert-true (= (markup-stream-level html-stream) *markup-stream-paragraph-level*))
  (assert-true (and paragraph-style (symbolp paragraph-style)))
  (let* ((anchors (list 'anchors))
         (logical-position (make-logical-position))
         (inner-html-stream (make-html-stream (markup-stream-env html-stream)
                                              *markup-stream-content-level*
                                              (- (html-stream-logical-line-width html-stream) (markup-width html-stream paragraph-style))
                                              nil
                                              logical-position
                                              (cons paragraph-style (html-stream-enclosing-styles html-stream))
                                              anchors)))
    (prog1
      (funcall emitter inner-html-stream)
      (markup-stream-append1 html-stream (cons paragraph-style
                                               (nreconc (cdr anchors)
                                                        (markup-stream-unexpanded-output inner-html-stream))))
      (assert-true (and (eq logical-position (html-stream-logical-position inner-html-stream))
                        (null (logical-position-n-soft-breaks logical-position))))
      (increment-division-length html-stream (if (= (logical-position-n-hard-breaks logical-position) 0)
                                               (1+ (logical-position-position logical-position))
                                               t)))))


(defmethod depict-char-style-f ((html-stream html-stream) char-style emitter)
  (assert-true (>= (markup-stream-level html-stream) *markup-stream-content-level*))
  (if char-style
    (progn
      (assert-true (symbolp char-style))
      (let ((inner-html-stream (make-html-stream (markup-stream-env html-stream)
                                                 *markup-stream-content-level*
                                                 (html-stream-logical-line-width html-stream)
                                                 nil
                                                 (markup-stream-logical-position html-stream)
                                                 (cons char-style (html-stream-enclosing-styles html-stream))
                                                 (html-stream-anchors html-stream))))
        (markup-stream-append1 inner-html-stream char-style)
        (prog1
          (funcall emitter inner-html-stream)
          (markup-stream-append1 html-stream (markup-stream-unexpanded-output inner-html-stream)))))
    (funcall emitter html-stream)))


(defmethod ensure-no-enclosing-style ((html-stream html-stream) style)
  (when (member style (html-stream-enclosing-styles html-stream))
    (cerror "Ignore" "Style ~S should not be in effect" style)))


(defmethod save-division-style ((html-stream html-stream))
  (reverse (html-stream-enclosing-styles html-stream)))


(defmethod with-saved-division-style-f ((html-stream html-stream) saved-division-style flatten emitter)
  (assert-true (<= (markup-stream-level html-stream) *markup-stream-paragraph-level*))
  (if (endp saved-division-style)
    (funcall emitter html-stream)
    (depict-division-style-f html-stream (first saved-division-style) flatten
                             #'(lambda (html-stream)
                                 (with-saved-division-style-f html-stream (rest saved-division-style) flatten emitter)))))


(defmethod depict-anchor ((html-stream html-stream) link-prefix link-name duplicate)
  (assert-true (= (markup-stream-level html-stream) *markup-stream-content-level*))
  (let* ((links (markup-env-links (html-stream-env html-stream)))
         (name (record-link-definition links link-prefix link-name duplicate)))
    (when name
      (push (list (list 'a (list 'name name))) (cdr (html-stream-anchors html-stream))))))


(defmethod depict-link-reference-f ((html-stream html-stream) link-prefix link-name external emitter)
  (assert-true (= (markup-stream-level html-stream) *markup-stream-content-level*))
  (let* ((links (markup-env-links (html-stream-env html-stream)))
         (href (record-link-reference links link-prefix link-name external)))
    (if href
      (let ((inner-html-stream (make-html-stream (markup-stream-env html-stream)
                                                 *markup-stream-content-level*
                                                 (html-stream-logical-line-width html-stream)
                                                 nil
                                                 (markup-stream-logical-position html-stream)
                                                 (html-stream-enclosing-styles html-stream)
                                                 (html-stream-anchors html-stream))))
        (markup-stream-append1 inner-html-stream (list 'a (list 'href href)))
        (prog1
          (funcall emitter inner-html-stream)
          (markup-stream-append1 html-stream (markup-stream-unexpanded-output inner-html-stream))))
      (funcall emitter html-stream))))


#|
(write-html
 '(html
   (head
    (:nowrap (title "This is my title!<>")))
   ((body (atr1 "abc") (beta) (qq))
    "My page this  is  " (br) (p))))
|#
