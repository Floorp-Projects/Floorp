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
;;; Custom HTML-to-RTF Converter
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(defconstant *missing-marker* "*****")


; Return the html-name-token of the tag of the given html element.
(defun tag-name (element)
  (html-parser:name (instance-of element)))


(defun match-tag-name (element tag-name)
  (eq (tag-name element) tag-name))


; Return the value of the given attribute in <element> or nil if not found.
(defun attribute-value (element attribute-name)
  (cdr (assoc attribute-name (attr-values element) :key #'html-parser:name)))


; Return true if the element has the given given <tag-name>, all of required-attributes, and perhaps
; the optional-attributes.
(defun match-element (element tag-name required-attributes optional-attributes)
  (and (match-tag-name element tag-name)
       (let ((attribute-values (attr-values element)))
         (and
          (every #'(lambda (required-attribute)
                     (assoc required-attribute attribute-values :key #'html-parser:name))
                 required-attributes)
          (every #'(lambda (attribute-value)
                     (let ((attribute (html-parser:name (car attribute-value))))
                       (or (member attribute required-attributes)
                           (member attribute optional-attributes))))
                 attribute-values)))))


; Ensure that <element> has the given given <tag-name>, all of required-attributes, and perhaps
; the optional-attributes.
(defun ensure-element (element tag-name required-attributes optional-attributes)
  (unless (match-element element tag-name required-attributes optional-attributes)
    (error "Tag ~S ~S ~S expected; got ~S" tag-name required-attributes optional-attributes element)))


; Return the children of <element> that have the given <tag-name>, all of required-attributes, and perhaps
; the optional-attributes.
(defun matching-parts (element tag-name required-attributes optional-attributes)
  (remove-if-not #'(lambda (child) (match-element child tag-name required-attributes optional-attributes))
                 (parts element)))


; Return the unique child of <element> that has the given <tag-name>, all of required-attributes, and perhaps
; the optional-attributes.
(defun matching-part (element tag-name required-attributes optional-attributes)
  (let ((parts (matching-parts element tag-name required-attributes optional-attributes)))
    (unless (and parts (endp (cdr parts)))
      (error "Element ~S should have only one ~S child" element tag-name))
    (car parts)))


; Convert control characters in the given string into spaces.
(defun normalize (string)
  (let ((l nil))
    (dotimes (i (length string))
      (let ((ch (char string i)))
        (if (<= (char-code ch) 32)
          (unless (eql (car l) #\Space)
            (push #\Space l))
          (push ch l))))
    (coerce (nreverse l) 'string)))


(defun normalize-preformatted (string)
  (map 'list #'(lambda (ch)
                 (if (< (char-code ch) 32)
                   'line
                   (string ch)))
       string))


(defvar *preformatted* nil)

(defun emit-string (markup-stream string)
  (if *preformatted*
    (dolist (segment (normalize-preformatted string))
      (depict markup-stream segment))
    (depict markup-stream (normalize string))))


(defparameter *special-char-code-map*
  '((#x00AB . :left-angle-quote)
    (#x00BB . :right-angle-quote)
    (#x2018 . :left-single-quote)
    (#x2019 . :right-single-quote)
    (#x201C . :left-double-quote)
    (#x201D . :right-double-quote)))


(defun emit-special-character (markup-stream char-num)
  (let ((code (cdr (assoc char-num *special-char-code-map*))))
    (if code
      (depict markup-stream code)
      (progn
        (depict markup-stream *missing-marker*)
        (format *terminal-io* "Ignoring character code ~S~%" char-num)))))


(defparameter *character-style-map*
  '(("control" . :character-literal-control)
    ("terminal" . :terminal)
    ("terminal-keyword" . :terminal-keyword)
    ("nonterminal" . :nonterminal)
    ("nonterminal-attribute" . :nonterminal-attribute)
    ("nonterminal-argument" . :nonterminal-argument)
    ("semantic-keyword" . :semantic-keyword)
    ("type-expression" . :type-expression)
    ("type-name" . :type-name)
    ("field-name" . :field-name)
    ("global-variable" . :global-variable)
    ("local-variable" . :local-variable)
    ("action-name" . :action-name)
    ("sub" . sub)
    ("sub-num" . :plain-subscript)))


(defun class-to-character-style (element)
  (let ((class (attribute-value element '#t"CLASS")))
    (if (null class)
      nil
      (let ((style (cdr (assoc class *character-style-map* :test #'equal))))
        (unless style
          (format *terminal-io* "Ignoring character style ~S~%" class))
        style))))


(defparameter *u-styles*
  '(("U_bull" . :bullet)
    ("U_ne" . :not-equal)
    ("U_le" . :less-or-equal)
    ("U_ge" . :greater-or-equal)
    ("U_infin" . :infinity)
    ("U_perp" . :bottom-10)
    ("U_larr" . :vector-assign-10)
    ("U_uarr" . :up-arrow-10)
    ("U_rarr" . :function-arrow-10)
    ("U_times" . :cartesian-product-10)
    ("U_equiv" . :identical-10)
    ("U_oplus" . :circle-plus-10)
    ("U_empty" . :empty-10)
    ("U_cap" . :intersection-10)
    ("U_cup" . :union-10)
    ("U_isin" . :member-10)
    ("U_notin" . :not-member-10)
    ("U_rArr" . :derives-10)
    ("U_lang" . :left-triangle-bracket-10)
    ("U_rang" . :right-triangle-bracket-10)
    
    ("U_alpha" . :alpha)
    ("U_beta" . :beta)
    ("U_chi" . :chi)
    ("U_delta" . :delta)
    ("U_epsilon" . :epsilon)
    ("U_phi" . :phi)
    ("U_gamma" . :gamma)
    ("U_eta" . :eta)
    ("U_iota" . :iota)
    ("U_kappa" . :kappa)
    ("U_lambda" . :lambda)
    ("U_mu" . :mu)
    ("U_nu" . :nu)
    ("U_omicron" . :omicron)
    ("U_pi" . :pi)
    ("U_theta" . :theta)
    ("U_rho" . :rho)
    ("U_sigma" . :sigma)
    ("U_tau" . :tau)
    ("U_upsilon" . :upsilon)
    ("U_omega" . :omega)
    ("U_xi" . :xi)
    ("U_psi" . :psi)
    ("U_zeta" . :zeta)))

(defun emit-script-element (markup-stream element)
  (let* ((children (parts element))
         (child (first children)))
    (if (and
         (= (length children) 1)
         (stringp child)
         (> (length child) 16)
         (equal (subseq child 0 15) "document.write(")
         (eql (char child (1- (length child))) #\)))
      (let* ((u-name (subseq child 15 (1- (length child))))
             (u-style (cdr (assoc u-name *u-styles* :test #'equal))))
        (if u-style
          (depict markup-stream u-style)
          (progn
            (depict markup-stream *missing-marker*)
            (format *terminal-io* "Ignoring SCRIPT element ~S ~S~%" element child))))
      (progn
        (depict markup-stream *missing-marker*)
        (format *terminal-io* "Ignoring SCRIPT element ~S ~S~%" element children)))))


(defparameter *entity-map*
  '((#e"nbsp" . ~)
    (#e"lt" . "<")
    (#e"gt" . ">")
    (#e"amp" . "&")
    (#e"quot" . "\"")))

(defun emit-entity (markup-stream entity)
  (let ((rtf (cdr (assoc entity *entity-map*))))
    (if rtf
      (depict markup-stream rtf)
      (progn
        (depict markup-stream "*****[" (html-parser:token-name entity) "]")
        (format *terminal-io* "Ignoring entity ~S~%" entity)))))


(defparameter *inline-element-map*
  '((#t"VAR" . :variable)
    (#t"B" . b)
    (#t"I" . i)
    (#t"TT" . :courier)
    (#t"SUB" . sub)))

(defun emit-inline-element (markup-stream element)
  (cond
   ((stringp element)
    (emit-string markup-stream element))
   ((integerp element)
    (emit-special-character markup-stream element))
   ((typep element 'html-entity-token)
    (emit-entity markup-stream element))
   ((match-element element '#t"SCRIPT" '(#t"TYPE") nil)
    (emit-script-element markup-stream element))
   ((or
     (match-element element '#t"A" nil '(#t"CLASS" #t"HREF" #t"NAME"))
     (match-element element '#t"SPAN" nil '(#t"CLASS")))
    (depict-char-style (markup-stream (class-to-character-style element))
      (emit-inline-elements markup-stream element)))
   ((match-element element '#t"CODE" nil '(#t"CLASS"))
    (let ((class (attribute-value element '#t"CLASS")))
      (if (equal class "terminal-keyword")
        (depict-char-style (markup-stream (class-to-character-style element))
          (emit-inline-elements markup-stream element))
        (progn
          (when class
            (format *terminal-io* "Ignoring CODE character style ~S~%" class))
          (depict-char-style (markup-stream :character-literal)
            (emit-inline-elements markup-stream element))))))
   ((match-element element '#t"SUP" nil '(#t"CLASS"))
    (depict-char-style (markup-stream 'super)
      (depict-char-style (markup-stream (class-to-character-style element))
        (emit-inline-elements markup-stream element))))
   (t (let ((inline-style (cdr (assoc (tag-name element) *inline-element-map*))))
        (if (and inline-style (endp (attr-values element)))
          (depict-char-style (markup-stream inline-style)
            (emit-inline-elements markup-stream element))
          (progn
            (depict markup-stream *missing-marker*)
            (format *terminal-io* "Ignoring inline element ~S~%" element)))))))


; Emit the children of the given element as inline elements.
(defun emit-inline-elements (markup-stream element)
  (dolist (child (parts element))
    (emit-inline-element markup-stream child)))



(defparameter *class-paragraph-styles*
  '(("mod-date" . :mod-date)
    ("grammar-argument" . :grammar-argument)))


(defun class-to-paragraph-style (element)
  (let ((class (attribute-value element '#t"CLASS")))
    (if class
      (let ((style (cdr (assoc class *class-paragraph-styles* :test #'equal))))
        (or style
            (progn
              (format *terminal-io* "Ignoring paragraph style ~S~%" class)
              :body-text)))
      :body-text)))


(defun grammar-rule-child-style (element last)
  (and
   (match-element element '#t"DIV" '(#t"CLASS") nil)
   (let ((class (attribute-value element '#t"CLASS")))
     (cond
      ((equal class "grammar-lhs")
       (if last :grammar-lhs-last :grammar-lhs))
      ((equal class "grammar-rhs")
       (if last :grammar-rhs-last :grammar-rhs))
      (t nil)))))


(defparameter *divs-containing-divs*
  '("indent"))

(defun emit-div (markup-stream element class)
  (cond
   ((equal class "grammar-rule")
    (let ((children (parts element)))
      (do ()
          ((endp children))
        (let* ((child (pop children))
               (style (grammar-rule-child-style child (endp children))))
          (unless style
            (format *terminal-io* "Bad grammar-rule child ~S~%" child)
            (setq style :body-text))
          (depict-paragraph (markup-stream style)
            (emit-inline-elements markup-stream child))))))
   ((member class *divs-containing-divs* :test #'equal)
    (depict-paragraph (markup-stream :body-text)
      (depict markup-stream "***** BEGIN DIV" class))
    (emit-paragraph-elements markup-stream element)
    (depict-paragraph (markup-stream :body-text)
      (depict markup-stream "***** END DIV" class)))
   (t (depict-paragraph (markup-stream (class-to-paragraph-style element))
        (emit-inline-elements markup-stream element)))))


(defparameter *paragraph-element-map*
  '((#t"H1" . :heading1)
    (#t"H2" . :heading2)
    (#t"H3" . :heading3)
    (#t"H4" . :heading4)))


; Emit the paragraph-level element.
(defun emit-paragraph-element (markup-stream element)
  (cond
   ((or
     (match-element element '#t"P" nil '(#t"CLASS"))
     (match-element element '#t"TH" nil '(#t"CLASS" #t"COLSPAN" #t"NOWRAP" #t"VALIGN" #t"ALIGN"))
     (match-element element '#t"TD" nil '(#t"CLASS" #t"COLSPAN" #t"NOWRAP" #t"VALIGN" #t"ALIGN")))
    (depict-paragraph (markup-stream (class-to-paragraph-style element))
      (emit-inline-elements markup-stream element)))
   ((match-element element '#t"PRE" nil nil)
    (depict-paragraph (markup-stream :sample-code)
      (let ((*preformatted* t))
        (emit-inline-elements markup-stream element))))
   ((match-element element '#t"UL" nil nil)
    (depict-paragraph (markup-stream :body-text)
      (depict markup-stream "***** BEGIN UL"))
    (dolist (child (parts element))
      (ensure-element child '#t"LI" nil nil)
      (depict-paragraph (markup-stream :body-text)
        (emit-inline-elements markup-stream child)))
    (depict-paragraph (markup-stream :body-text)
      (depict markup-stream "***** END UL")))
   ((match-element element '#t"DIV" nil '(#t"CLASS"))
    (let ((class (attribute-value element '#t"CLASS")))
      (if class
        (emit-div markup-stream element class)
        (emit-paragraph-elements markup-stream element))))
   ((match-element element '#t"HR" nil nil))
   ((match-element element '#t"TABLE" nil '(#t"BORDER" #t"CELLSPACING" #t"CELLPADDING"))
    (depict-paragraph (markup-stream :body-text)
      (depict markup-stream "***** BEGIN TABLE"))
    (emit-paragraph-elements markup-stream element)
    (depict-paragraph (markup-stream :body-text)
      (depict markup-stream "***** END TABLE")))
   ((match-element element '#t"THEAD" nil nil)
    (emit-paragraph-elements markup-stream element))
   ((match-element element '#t"TR" nil nil)
    (emit-paragraph-elements markup-stream element))
   (t (let ((paragraph-style (cdr (assoc (tag-name element) *paragraph-element-map*))))
        (if (and paragraph-style (endp (attr-values element)))
          (depict-paragraph (markup-stream paragraph-style)
            (emit-inline-elements markup-stream element))
          (progn
            (depict-paragraph (markup-stream :body-text)
              (depict markup-stream *missing-marker*))
            (format *terminal-io* "Ignoring paragraph element ~S~%" element)))))))


; Emit the children of the given element as paragraph-level elements.
(defun emit-paragraph-elements (markup-stream element)
  (dolist (child (parts element))
    (emit-paragraph-element markup-stream child)))


(defun emit-html-file (markup-stream element)
  (ensure-element element '#t"HTML" nil nil)
  (let* ((body (matching-part element '#t"BODY" nil nil))
         (body-elements (parts body)))
    (when (and body-elements (match-tag-name (first body-elements) '#t"TABLE"))
      (setq body-elements (rest body-elements)))
    (when (and body-elements (match-tag-name (car (last body-elements)) '#t"TABLE"))
      (setq body-elements (butlast body-elements)))
    (dolist (body-element body-elements)
      (emit-paragraph-element markup-stream body-element))))


(defun translate-html-to-rtf (html-file-name rtf-path title)
  (let* ((source-text (file->string html-file-name))
         (element (html-parser::simple-parser source-text)))
    (depict-rtf-to-local-file
     rtf-path
     title
     #'(lambda (markup-stream)
         (emit-html-file markup-stream element))
     *html-to-rtf-definitions*)))

#|
(setq s (html-parser:file->string "Huit:Mozilla:Docs:mozilla-org:html:js:language:js20:formal:index.html"))
(setq p (html-parser::simple-parser s))

(depict-rtf-to-local-file
 "HTML-To-RTF/Test.rtf"
 "Test"
 #'(lambda (markup-stream)
     (emit-html-file markup-stream p))
 *html-to-rtf-definitions*)

(translate-html-to-rtf "Huit:Mozilla:Docs:mozilla-org:html:js:language:js20:formal:index.html" "HTML-To-RTF/Test.rtf" "Test")
(translate-html-to-rtf "Huit:Mozilla:Docs:mozilla-org:html:js:language:js20:introduction:notation.html"
                       "HTML-To-RTF/Notation.rtf" "Notation")
(translate-html-to-rtf "Huit:Mozilla:Docs:mozilla-org:html:js:language:es4:core:expressions.html"
                       "HTML-To-RTF/Expressions.rtf" "Expressions")
(translate-html-to-rtf "Huit:Mozilla:Moz:mozilla:js2:semantics:HTML-To-RTF:Expressions.html"
                       "HTML-To-RTF/Expressions.rtf" "Expressions")
|#
