;;;
;;; JavaScript 2.0 unit lexer
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(progn
  (defparameter *uw*
    (generate-world
     "U"
     '((lexer unit-lexer
              :lalr-1
              :unit-pattern
              ((:unicode-initial-alphabetic
                (% initial-alpha (:text "Any Unicode initial alphabetic character (includes ASCII "
                                        (:character-literal #\A) :nbhy (:character-literal #\Z) " and "
                                        (:character-literal #\a) :nbhy (:character-literal #\z) ")"))
                () t)
               (:unicode-alphanumeric
                (% alphanumeric (:text "Any Unicode alphabetic or decimal digit character (includes ASCII "
                                       (:character-literal #\0) :nbhy (:character-literal #\9) ", "
                                       (:character-literal #\A) :nbhy (:character-literal #\Z) ", and "
                                       (:character-literal #\a) :nbhy (:character-literal #\z) ")"))
                () t)
               (:white-space-character (++ (#?0009 #?000B #?000C #\space #?00A0)
                                           (#?2000 #?2001 #?2002 #?2003 #?2004 #?2005 #?2006 #?2007)
                                           (#?2008 #?2009 #?200A #?200B)
                                           (#?3000)) ())
               (:line-terminator (#?000A #?000D #?2028 #?2029) ())
               (:initial-identifier-character (+ :unicode-initial-alphabetic (#\$ #\_))
                                              (($default-action $default-action)))
               (:continuing-identifier-character (+ :unicode-alphanumeric (#\$ #\_))
                                                 (($default-action $default-action)))
               (:a-s-c-i-i-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                 (($default-action $default-action)
                                  (decimal-value $digit-value))))
              (($default-action character nil identity)
               ($digit-value integer digit-value digit-char-36)))
       
       
       (%text nil "The start nonterminal is " (:grammar-symbol :unit-pattern) ".")
       
       (%print-actions)
       
       
       (%heading 1 "White Space")
       
       (grammar-argument :sigma wsopt wsreq)
       
       (%charclass :white-space-character)
       (%charclass :line-terminator)
       
       (production :required-white-space (:white-space-character) required-white-space-character)
       (production :required-white-space (:line-terminator) required-white-space-line-terminator)
       (production :required-white-space (:required-white-space :white-space-character) required-white-space-more-character)
       (production :required-white-space (:required-white-space :line-terminator) required-white-space-more-line-terminator)
       
       (production (:white-space :sigma) (:required-white-space) white-space-required-white-space)
       (production (:white-space wsopt) () white-space-empty)
       
       (%heading 1 "Unit Patterns")
       
       (rule :unit-pattern ((value unit-list))
         (production :unit-pattern ((:white-space wsopt) :unit-quotient) unit-pattern-quotient
           (value (value :unit-quotient))))
       
       (rule :unit-quotient ((value unit-list))
         (production :unit-quotient ((:unit-product wsopt)) unit-quotient-product
           (value (value :unit-product)))
         (production :unit-quotient ((:unit-product wsopt) #\/ (:white-space wsopt) (:unit-product wsopt)) unit-quotient-quotient
           (value (append (value :unit-product 1) (unit-reciprocal (value :unit-product 2))))))
       
       (rule (:unit-product :sigma) ((value unit-list))
         (production (:unit-product :sigma) ((:unit-factor :sigma)) unit-product-factor
           (value (value :unit-factor)))
         (production (:unit-product :sigma) ((:unit-product wsopt) #\* (:white-space wsopt) (:unit-factor :sigma)) unit-product-product
           (value (append (value :unit-product) (value :unit-factor))))
         (production (:unit-product :sigma) ((:unit-product wsreq) (:unit-factor :sigma)) unit-product-implied-product
           (value (append (value :unit-product) (value :unit-factor)))))
       
       (rule (:unit-factor :sigma) ((value unit-list))
         (production (:unit-factor :sigma) (#\1 (:white-space :sigma)) unit-factor-one
           (value (vector-of unit-factor)))
         (production (:unit-factor :sigma) (#\1 (:white-space wsopt) #\^ (:white-space wsopt) :signed-integer (:white-space :sigma)) unit-factor-one-exponent
           (value (vector-of unit-factor)))
         (production (:unit-factor :sigma) (:identifier (:white-space :sigma)) unit-factor-identifier
           (value (vector (new unit-factor (name :identifier) 1))))
         (production (:unit-factor :sigma) (:identifier (:white-space wsopt) #\^ (:white-space wsopt) :signed-integer (:white-space :sigma)) unit-factor-identifier-exponent
           (value (vector (new unit-factor (name :identifier) (integer-value :signed-integer))))))
       
       (deftuple unit-factor (identifier string) (exponent integer))
       (deftype unit-list (vector unit-factor))
       
       (define (unit-reciprocal (value unit-list)) unit-list
         (return (map value f (new unit-factor (& identifier f) (neg (& exponent f))))))
       
       (%print-actions)
       
       
       (%heading 1 "Signed Integers")
       (rule :signed-integer ((integer-value integer))
         (production :signed-integer (:decimal-digits) signed-integer-no-sign
           (integer-value (integer-value :decimal-digits)))
         (production :signed-integer (#\+ :decimal-digits) signed-integer-plus
           (integer-value (integer-value :decimal-digits)))
         (production :signed-integer (#\- :decimal-digits) signed-integer-minus
           (integer-value (neg (integer-value :decimal-digits)))))
       
       (rule :decimal-digits ((integer-value integer))
         (production :decimal-digits (:a-s-c-i-i-digit) decimal-digits-first
           (integer-value (decimal-value :a-s-c-i-i-digit)))
         (production :decimal-digits (:decimal-digits :a-s-c-i-i-digit) decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :decimal-digits)) (decimal-value :a-s-c-i-i-digit)))))
       
       (%charclass :a-s-c-i-i-digit)
       (%print-actions)
       
       
       (%heading 1 "Identifiers")
       (rule :identifier ((name string))
         (production :identifier (:initial-identifier-character) identifier-initial
           (name (vector ($default-action :initial-identifier-character))))
         (production :identifier (:identifier :continuing-identifier-character) identifier-continuing
           (name (append (name :identifier) (vector ($default-action :continuing-identifier-character))))))
       
       (%charclass :initial-identifier-character)
       (%charclass :continuing-identifier-character)
       (%charclass :unicode-initial-alphabetic)
       (%charclass :unicode-alphanumeric)
       (%print-actions)
       )))
  
  (defparameter *ul* (world-lexer *uw* 'unit-lexer))
  (defparameter *ug* (lexer-grammar *ul*))
  (set-up-lexer-metagrammar *ul*)
  (defparameter *um* (lexer-metagrammar *ul*)))


(defun dump-units ()
  (values
   (depict-rtf-to-local-file
    "JS20/UnitGrammar.rtf"
    "JavaScript 2 Unit Grammar"
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *uw* :heading-offset 1 :visible-semantics nil)))
   (depict-rtf-to-local-file
    "JS20/UnitSemantics.rtf"
    "JavaScript 2 Unit Semantics"
    #'(lambda (rtf-stream)
        (depict-world-commands rtf-stream *uw* :heading-offset 1)))
   (depict-html-to-local-file
    "JS20/UnitGrammar.html"
    "JavaScript 2 Unit Grammar"
    t
    #'(lambda (html-stream)
        (depict-world-commands html-stream *uw* :heading-offset 1 :visible-semantics nil))
    :external-link-base "notation.html")
   (depict-html-to-local-file
    "JS20/UnitSemantics.html"
    "JavaScript 2 Unit Semantics"
    t
    #'(lambda (html-stream)
        (depict-world-commands html-stream *uw* :heading-offset 1))
    :external-link-base "notation.html")))

#|
(dump-units)

(depict-rtf-to-local-file
 "JS20/UnitCharClasses.rtf"
 "JavaScript 2 Unit Character Classes"
 #'(lambda (rtf-stream)
     (depict-paragraph (rtf-stream :grammar-header)
       (depict rtf-stream "Character Classes"))
     (dolist (charclass (lexer-charclasses *ul*))
       (depict-charclass rtf-stream charclass))
     (depict-paragraph (rtf-stream :grammar-header)
       (depict rtf-stream "Grammar"))
     (depict-grammar rtf-stream *ug*)))


(with-local-output (s "JS20/UnitGrammar.txt") (print-lexer *ul* s) (print-grammar *ug* s))

(print-illegal-strings m)
|#


#+allegro (clean-grammar *ug*) ;Remove this line if you wish to print the grammar's state tables.
(length (grammar-states *ug*))
