;;;
;;; JavaScript 2.0 regular expression parser
;;;
;;; Waldemar Horwat (waldemar@acm.org)
;;;


(progn
  (defparameter *rw*
    (generate-world
     "R"
     '((lexer regexp-lexer
              :lr-1
              :regular-expression-pattern
              ((:unicode-character (% every (:text "Any Unicode character")) () t)
               (:unicode-alphanumeric
                (% alphanumeric (:text "Any Unicode alphabetic or decimal digit character (includes ASCII "
                                       (:character-literal #\0) :nbhy (:character-literal #\9) ", "
                                       (:character-literal #\A) :nbhy (:character-literal #\Z) ", and "
                                       (:character-literal #\a) :nbhy (:character-literal #\z) ")"))
                () t)
               (:line-terminator (#?000A #?000D #?2028 #?2029) () t)
               (:decimal-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                               (($default-action $default-action)
                                (decimal-value $digit-value)))
               (:non-zero-digit (#\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)
                                ((decimal-value $digit-value)))
               (:hex-digit (#\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9 #\A #\B #\C #\D #\E #\F #\a #\b #\c #\d #\e #\f)
                           ((hex-value $digit-value)))
               (:control-letter (++ (#\A #\B #\C #\D #\E #\F #\G #\H #\I #\J #\K #\L #\M #\N #\O #\P #\Q #\R #\S #\T #\U #\V #\W #\X #\Y #\Z)
                                    (#\a #\b #\c #\d #\e #\f #\g #\h #\i #\j #\k #\l #\m #\n #\o #\p #\q #\r #\s #\t #\u #\v #\w #\x #\y #\z))
                                (($default-action $default-action)))
               (:pattern-character (- :unicode-character (#\^ #\$ #\\ #\. #\* #\+ #\? #\( #\) #\[ #\] #\{ #\} #\|))
                                   (($default-action $default-action)))
               ((:class-character dash) (- :unicode-character (#\\ #\]))
                (($default-action $default-action)))
               ((:class-character no-dash) (- (:class-character dash) (#\-))
                (($default-action $default-action)))
               (:identity-escape (- :unicode-character :unicode-alphanumeric)
                                 (($default-action $default-action))))
              (($default-action character nil identity)
               ($digit-value integer digit-value digit-char-36)))
       
       (deftype semantic-exception (oneof syntax-error))
       
       (%section "Unicode Character Classes")
       (%charclass :unicode-character)
       (%charclass :unicode-alphanumeric)
       (%charclass :line-terminator)
       
       (define line-terminators (set character) (set-of character #?000A #?000D #?2028 #?2029))
       (define re-whitespaces (set character) (set-of character #?000C #?000A #?000D #?0009 #?000B #\space))
       (define re-digits (set character) (set-of-ranges character #\0 #\9))
       (define re-word-characters (set character) (set-of-ranges character #\0 #\9 #\A #\Z #\a #\z #\_ nil))
       (%print-actions)
       
       
       (%section "Regular Expression Definitions")
       (deftype r-e-input (tuple (str string) (ignore-case boolean) (multiline boolean)))
       (%text :semantics
         "Field " (:field str r-e-input) " is the input string. "
         (:field ignore-case r-e-input) " and " (:field multiline r-e-input) " are the corresponding regular expression flags.")
       
       (deftype r-e-result (oneof (success r-e-match) failure))
       (deftype r-e-match (tuple (end-index integer)
                            (captures (vector capture))))
       (%text :semantics
         "A " (:type r-e-match) " holds an intermediate state during the pattern-matching process. "
         (:field end-index r-e-match)
         " is the index of the next input character to be matched by the next component in a regular expression pattern. "
         "If we are at the end of the pattern, " (:field end-index r-e-match)
         " is one plus the index of the last matched input character. "
         (:field captures r-e-match)
         " is a zero-based array of the strings captured so far by capturing parentheses.")
       
       (deftype capture (oneof (present string)
                               absent))
       (deftype continuation (-> (r-e-match) r-e-result))
       (%text :semantics
         "A " (:type continuation)
         " is a function that attempts to match the remaining portion of the pattern against the input string, "
         "starting at the intermediate state given by its " (:type r-e-match) " argument. "
         "If a match is possible, it returns a " (:field success r-e-result) " result that contains the final "
         (:type r-e-match) " state; if no match is possible, it returns a " (:field failure r-e-result) " result.")
       
       (deftype matcher (-> (r-e-input r-e-match continuation) r-e-result))
       (%text :semantics
         "A " (:type matcher)
         " is a function that attempts to match a middle portion of the pattern against the input string, "
         "starting at the intermediate state given by its " (:type r-e-match) " argument. "
         "Since the remainder of the pattern heavily influences whether (and how) a middle portion will match, we "
         "must pass in a " (:type continuation) " function that checks whether the rest of the pattern matched. "
         "If the continuation returns " (:field failure r-e-result) ", the matcher function may call it repeatedly, "
         "trying various alternatives at pattern choice points.")
       (%text :semantics
         "The " (:type r-e-input) " parameter contains the input string and is merely passed down to subroutines.")
       
       (deftype matcher-generator (-> (integer) matcher))
       (%text :semantics
         "A " (:type matcher-generator)
         " is a function executed at the time the regular expression is compiled that returns a " (:type matcher) " for a part "
         "of the pattern. The " (:type integer) " parameter contains the number of capturing left parentheses seen so far in the "
         "pattern and is used to assign static, consecutive numbers to capturing parentheses.")
       
       (define (character-set-matcher (acceptance-set (set character)) (invert boolean)) matcher   ;*********ignore case?
         (function ((t r-e-input) (x r-e-match) (c continuation))
           (let ((i integer (& end-index x))
                 (s string (& str t)))
             (if (= i (length s))
               (oneof failure)
               (if (xor (character-set-member (nth s i) acceptance-set) invert)
                 (c (tuple r-e-match (+ i 1) (& captures x)))
                 (oneof failure))))))
       (%text :semantics
         (:global character-set-matcher) " returns a " (:type matcher)
         " that matches a single input string character. If "
         (:local invert) " is false, the match succeeds if the character is a member of the "
         (:local acceptance-set) " set of characters (possibly ignoring case). If "
         (:local invert) " is true, the match succeeds if the character is not a member of the "
         (:local acceptance-set) " set of characters (possibly ignoring case).")
       
       (define (character-matcher (ch character)) matcher
         (character-set-matcher (set-of character ch) false))
       (%text :semantics
         (:global character-matcher) " returns a " (:type matcher)
         " that matches a single input string character. The match succeeds if the character is the same as "
         (:local ch) " (possibly ignoring case).")
       
       (%print-actions)
       
       
       (%section "Regular Expression Patterns")
       
       (rule :regular-expression-pattern ((exec (-> (r-e-input integer) r-e-result)))
         (production :regular-expression-pattern (:disjunction) regular-expression-pattern-disjunction
           (exec
            (let ((match matcher ((gen-matcher :disjunction) 0)))
              (function ((t r-e-input) (index integer))
                (match
                 t
                 (tuple r-e-match index (fill-capture (count-parens :disjunction)))
                 success-continuation))))))
       
       (%print-actions)
       (define (success-continuation (x r-e-match)) r-e-result
         (oneof success x))
       (define (fill-capture (i integer)) (vector capture)
         (if (= i 0)
           (vector-of capture)
           (append (fill-capture (- i 1)) (vector (oneof absent)))))
       
       
       (%subsection "Disjunctions")
       
       (rule :disjunction ((gen-matcher matcher-generator) (count-parens integer))
         (production :disjunction (:alternative) disjunction-one
           (gen-matcher (gen-matcher :alternative))
           (count-parens (count-parens :alternative)))
         (production :disjunction (:alternative #\| :disjunction) disjunction-more
           ((gen-matcher (paren-index integer))
            (let ((match1 matcher ((gen-matcher :alternative) paren-index))
                  (match2 matcher ((gen-matcher :disjunction) (+ paren-index (count-parens :alternative)))))
              (function ((t r-e-input) (x r-e-match) (c continuation))
                (case (match1 t x c)
                  ((success y r-e-match) (oneof success y))
                  (failure (match2 t x c))))))
           (count-parens (+ (count-parens :alternative) (count-parens :disjunction)))))
       
       (%print-actions)
       
       
       (%subsection "Alternatives")
       
       (rule :alternative ((gen-matcher matcher-generator) (count-parens integer))
         (production :alternative () alternative-none
           ((gen-matcher (paren-index integer :unused))
            (function ((t r-e-input :unused) (x r-e-match) (c continuation))
              (c x)))
           (count-parens 0))
         (production :alternative (:alternative :term) alternative-some
           ((gen-matcher (paren-index integer))
            (let ((match1 matcher ((gen-matcher :alternative) paren-index))
                  (match2 matcher ((gen-matcher :term) (+ paren-index (count-parens :alternative)))))
              (function ((t r-e-input) (x r-e-match) (c continuation))
                (let ((d continuation (function ((y r-e-match))
                                        (match2 t y c))))
                  (match1 t x d)))))
           (count-parens (+ (count-parens :alternative) (count-parens :term)))))
       
       (%print-actions)
       
       
       (%subsection "Terms")
       
       (rule :term ((gen-matcher matcher-generator) (count-parens integer))
         (production :term (:assertion) term-assertion
           ((gen-matcher (paren-index integer :unused))
            (function ((t r-e-input) (x r-e-match) (c continuation))
              (if ((test-assertion :assertion) t x)
                (c x)
                (oneof failure))))
           (count-parens 0))
         (production :term (:atom) term-atom
           (gen-matcher (gen-matcher :atom))
           (count-parens (count-parens :atom)))
         (production :term (:atom :quantifier) term-quantified-atom
           ((gen-matcher (paren-index integer))
            (let ((match matcher ((gen-matcher :atom) paren-index))
                  (min integer (minimum :quantifier))
                  (max limit (maximum :quantifier))
                  (greedy boolean (greedy :quantifier)))
              (if (case max
                    ((finite m integer) (< m min))
                    (infinite false))
                (throw (oneof syntax-error))
                (repeat-matcher match min max greedy paren-index (count-parens :atom)))))
           (count-parens (count-parens :atom))))
       
       (%print-actions)
       
       
       (rule :quantifier ((minimum integer) (maximum limit) (greedy boolean))
         (production :quantifier (:quantifier-prefix) quantifier-eager
           (minimum (minimum :quantifier-prefix))
           (maximum (maximum :quantifier-prefix))
           (greedy true))
         (production :quantifier (:quantifier-prefix #\?) quantifier-greedy
           (minimum (minimum :quantifier-prefix))
           (maximum (maximum :quantifier-prefix))
           (greedy false)))
       
       (rule :quantifier-prefix ((minimum integer) (maximum limit))
         (production :quantifier-prefix (#\*) quantifier-prefix-zero-or-more
           (minimum 0)
           (maximum (oneof infinite)))
         (production :quantifier-prefix (#\+) quantifier-prefix-one-or-more
           (minimum 1)
           (maximum (oneof infinite)))
         (production :quantifier-prefix (#\?) quantifier-prefix-zero-or-one
           (minimum 0)
           (maximum (oneof finite 1)))
         (production :quantifier-prefix (#\{ :decimal-digits #\}) quantifier-prefix-repeat
           (minimum (integer-value :decimal-digits))
           (maximum (oneof finite (integer-value :decimal-digits))))
         (production :quantifier-prefix (#\{ :decimal-digits #\, #\}) quantifier-prefix-repeat-or-more
           (minimum (integer-value :decimal-digits))
           (maximum (oneof infinite)))
         (production :quantifier-prefix (#\{ :decimal-digits #\, :decimal-digits #\}) quantifier-prefix-repeat-range
           (minimum (integer-value :decimal-digits 1))
           (maximum (oneof finite (integer-value :decimal-digits 2)))))
       
       (rule :decimal-digits ((integer-value integer))
         (production :decimal-digits (:decimal-digit) decimal-digits-first
           (integer-value (decimal-value :decimal-digit)))
         (production :decimal-digits (:decimal-digits :decimal-digit) decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :decimal-digits)) (decimal-value :decimal-digit)))))
       (%charclass :decimal-digit)
       
       
       (deftype limit (oneof (finite integer) infinite))
       
       (define (reset-parens (x r-e-match) (p integer) (n-parens integer)) r-e-match
         (if (= n-parens 0)
           x
           (let ((y r-e-match (tuple r-e-match (& end-index x)
                                     (set-nth (& captures x) p (oneof absent)))))
             (reset-parens y (+ p 1) (- n-parens 1)))))
       
       (define (repeat-matcher (body matcher) (min integer) (max limit) (greedy boolean) (paren-index integer) (n-body-parens integer)) matcher
         (function ((t r-e-input) (x r-e-match) (c continuation))
           (if (case max
                 ((finite m integer) (= m 0))
                 (infinite false))
             (c x)
             (let ((d continuation (function ((y r-e-match))
                                     (if (and (= min 0)
                                              (= (& end-index y) (& end-index x)))
                                       (oneof failure)
                                       (let ((new-min integer (if (= min 0) 0 (- min 1)))
                                             (new-max limit (case max
                                                              ((finite m integer) (oneof finite (- m 1)))
                                                              (infinite (oneof infinite)))))
                                         ((repeat-matcher body new-min new-max greedy paren-index n-body-parens) t y c)))))
                   (xr r-e-match (reset-parens x paren-index n-body-parens)))
               (if (/= min 0)
                 (body t xr d)
                 (if greedy
                   (case (body t xr d)
                     ((success z r-e-match) (oneof success z))
                     (failure (c x)))
                   (case (c x)
                     ((success z r-e-match) (oneof success z))
                     (failure (body t xr d)))))))))
       
       (%print-actions)
       
       
       (%subsection "Assertions")
       
       (rule :assertion ((test-assertion (-> (r-e-input r-e-match) boolean)))
         (production :assertion (#\^) assertion-beginning
           ((test-assertion (t r-e-input) (x r-e-match))
            (if (= (& end-index x) 0)
              true
              (and (& multiline t)
                   (character-set-member (nth (& str t) (- (& end-index x) 1)) line-terminators)))))
         (production :assertion (#\$) assertion-end
           ((test-assertion (t r-e-input) (x r-e-match))
            (if (= (& end-index x) (length (& str t)))
              true
              (and (& multiline t)
                   (character-set-member (nth (& str t) (& end-index x)) line-terminators)))))
         (production :assertion (#\\ #\b) assertion-word-boundary
           ((test-assertion (t r-e-input) (x r-e-match))
            (at-word-boundary (& end-index x) (& str t))))
         (production :assertion (#\\ #\B) assertion-non-word-boundary
           ((test-assertion (t r-e-input) (x r-e-match))
            (not (at-word-boundary (& end-index x) (& str t))))))
       
       (%print-actions)
       
       (define (at-word-boundary (i integer) (s string)) boolean
         (xor (in-word (- i 1) s) (in-word i s)))
       
       (define (in-word (i integer) (s string)) boolean
         (if (or (= i -1) (= i (length s)))
           false
           (character-set-member (nth s i) re-word-characters)))
       
       
       (%section "Atoms")
       
       (rule :atom ((gen-matcher matcher-generator) (count-parens integer))
         (production :atom (:pattern-character) atom-pattern-character
           ((gen-matcher (paren-index integer :unused))
            (character-matcher ($default-action :pattern-character)))
           (count-parens 0))
         (production :atom (#\.) atom-dot
           ((gen-matcher (paren-index integer :unused))
            (character-set-matcher line-terminators true))
           (count-parens 0))
         (production :atom (:null-escape) atom-null-escape
           ((gen-matcher (paren-index integer :unused))
            (function ((t r-e-input :unused) (x r-e-match) (c continuation))
              (c x)))
           (count-parens 0))
         (production :atom (#\\ :atom-escape) atom-atom-escape
           (gen-matcher (gen-matcher :atom-escape))
           (count-parens 0))
         (production :atom (:character-class) atom-character-class
           ((gen-matcher (paren-index integer :unused))
            (let ((a (set character) (acceptance-set :character-class)))
              (character-set-matcher a (invert :character-class))))
           (count-parens 0))
         (production :atom (#\( :disjunction #\)) atom-parentheses
           ((gen-matcher (paren-index integer))
            (let ((match matcher ((gen-matcher :disjunction) (+ paren-index 1))))
              (function ((t r-e-input) (x r-e-match) (c continuation))
                (let ((d continuation
                         (function ((y r-e-match))
                           (let ((updated-captures (vector capture)
                                                   (set-nth (& captures y) paren-index
                                                            (oneof present (subseq (& str t) (& end-index x) (- (& end-index y) 1))))))
                             (c (tuple r-e-match (& end-index y) updated-captures))))))
                  (match t x d)))))
           (count-parens (+ (count-parens :disjunction) 1)))
         (production :atom (#\( #\? #\: :disjunction #\)) atom-non-capturing-parentheses
           (gen-matcher (gen-matcher :disjunction))
           (count-parens (count-parens :disjunction)))
         (production :atom (#\( #\? #\= :disjunction #\)) atom-positive-lookahead
           ((gen-matcher (paren-index integer))
            (let ((match matcher ((gen-matcher :disjunction) paren-index)))
              (function ((t r-e-input) (x r-e-match) (c continuation))
                ;(let ((d continuation
                ;         (function ((y r-e-match))
                ;           (c (tuple r-e-match (& end-index x) (& captures y))))))
                ;  (match t x d)))))
                (case (match t x success-continuation)
                  ((success y r-e-match)
                   (c (tuple r-e-match (& end-index x) (& captures y))))
                  (failure (oneof failure))))))
           (count-parens (count-parens :disjunction)))
         (production :atom (#\( #\? #\! :disjunction #\)) atom-negative-lookahead
           ((gen-matcher (paren-index integer))
            (let ((match matcher ((gen-matcher :disjunction) paren-index)))
              (function ((t r-e-input) (x r-e-match) (c continuation))
                (case (match t x success-continuation)
                  ((success y r-e-match :unused) (oneof failure))
                  (failure (c x))))))
           (count-parens (count-parens :disjunction))))
       
       (%charclass :pattern-character)
       (%print-actions)
       
       
       (%section "Escapes")
       
       (production :null-escape (#\\ #\Q) null-escape-q)
       
       (rule :atom-escape ((gen-matcher matcher-generator))
         (production :atom-escape (:decimal-escape) atom-escape-decimal
           ((gen-matcher (paren-index integer))
            (let ((n integer (escape-value :decimal-escape)))
              (if (= n 0)
                (character-matcher #?0000)
                (if (> n paren-index)
                  (throw (oneof syntax-error))
                  (backreference-matcher n))))))
         (production :atom-escape (:character-escape) atom-escape-character
           ((gen-matcher (paren-index integer :unused))
            (character-matcher (character-value :character-escape))))
         (production :atom-escape (:character-class-escape) atom-escape-character-class
           ((gen-matcher (paren-index integer :unused))
            (character-set-matcher (acceptance-set :character-class-escape) false))))
       (%print-actions)
       
       (define (backreference-matcher (n integer)) matcher
         (function ((t r-e-input) (x r-e-match) (c continuation))
           (case (nth-backreference x n)
             ((present ref string)
              (let ((i integer (& end-index x))
                    (s string (& str t)))
                (let ((j integer (+ i (length ref))))
                  (if (> j (length s))
                    (oneof failure)
                    (if (string-equal (subseq s i (- j 1)) ref)   ;*********ignore case?
                      (c (tuple r-e-match j (& captures x)))
                      (oneof failure))))))
             (absent (c x)))))
       
       (define (nth-backreference (x r-e-match) (n integer)) capture
         (nth (& captures x) (- n 1)))
       
       
       (rule :character-escape ((character-value character))
         (production :character-escape (:control-escape) character-escape-control
           (character-value (character-value :control-escape)))
         (production :character-escape (#\c :control-letter) character-escape-control-letter
           (character-value (code-to-character (bitwise-and (character-to-code ($default-action :control-letter)) 31))))
         (production :character-escape (:hex-escape) character-escape-hex
           (character-value (character-value :hex-escape)))
         (production :character-escape (:identity-escape) character-escape-identity
           (character-value ($default-action :identity-escape))))
       
       (%charclass :control-letter)
       (%charclass :identity-escape)
       
       (rule :control-escape ((character-value character))
         (production :control-escape (#\f) control-escape-form-feed (character-value #?000C))
         (production :control-escape (#\n) control-escape-new-line (character-value #?000A))
         (production :control-escape (#\r) control-escape-return (character-value #?000D))
         (production :control-escape (#\t) control-escape-tab (character-value #?0009))
         (production :control-escape (#\v) control-escape-vertical-tab (character-value #?000B)))
       (%print-actions)
       
       
       (%subsection "Decimal Escapes")
       
       (rule :decimal-escape ((escape-value integer))
         (production :decimal-escape (:decimal-integer-literal (:- :decimal-digit)) decimal-escape-integer
           (escape-value (integer-value :decimal-integer-literal))))
       
       (rule :decimal-integer-literal ((integer-value integer))
         (production :decimal-integer-literal (#\0) decimal-integer-literal-0
           (integer-value 0))
         (production :decimal-integer-literal (:non-zero-decimal-digits) decimal-integer-literal-nonzero
           (integer-value (integer-value :non-zero-decimal-digits))))
       
       (rule :non-zero-decimal-digits ((integer-value integer))
         (production :non-zero-decimal-digits (:non-zero-digit) non-zero-decimal-digits-first
           (integer-value (decimal-value :non-zero-digit)))
         (production :non-zero-decimal-digits (:non-zero-decimal-digits :decimal-digit) non-zero-decimal-digits-rest
           (integer-value (+ (* 10 (integer-value :non-zero-decimal-digits)) (decimal-value :decimal-digit)))))
       
       (%charclass :non-zero-digit)
       (%print-actions)
       
       
       (%subsection "Hexadecimal Escapes")
       
       (rule :hex-escape ((character-value character))
         (production :hex-escape (#\x :hex-digit :hex-digit) hex-escape-2
           (character-value (code-to-character (+ (* 16 (hex-value :hex-digit 1))
                                                  (hex-value :hex-digit 2)))))
         (production :hex-escape (#\u :hex-digit :hex-digit :hex-digit :hex-digit) hex-escape-4
           (character-value (code-to-character (+ (+ (+ (* 4096 (hex-value :hex-digit 1))
                                                        (* 256 (hex-value :hex-digit 2)))
                                                     (* 16 (hex-value :hex-digit 3)))
                                                  (hex-value :hex-digit 4))))))
       (%charclass :hex-digit)
       (%print-actions)
       
       
       (%subsection "Character Class Escapes")
       
       (rule :character-class-escape ((acceptance-set (set character)))
         (production :character-class-escape (#\s) character-class-escape-whitespace
           (acceptance-set re-whitespaces))
         (production :character-class-escape (#\S) character-class-escape-non-whitespace
           (acceptance-set (character-set-difference (set-of-ranges character #?0000 #?FFFF) re-whitespaces)))
         (production :character-class-escape (#\d) character-class-escape-digit
           (acceptance-set re-digits))
         (production :character-class-escape (#\D) character-class-escape-non-digit
           (acceptance-set (character-set-difference (set-of-ranges character #?0000 #?FFFF) re-digits)))
         (production :character-class-escape (#\w) character-class-escape-word
           (acceptance-set re-word-characters))
         (production :character-class-escape (#\W) character-class-escape-non-word
           (acceptance-set (character-set-difference (set-of-ranges character #?0000 #?FFFF) re-word-characters))))
       (%print-actions)
       
       
       (%section "User-Specified Character Classes")
       
       (rule :character-class ((acceptance-set (set character)) (invert boolean))
         (production :character-class (#\[ (:- #\^) :class-ranges #\]) character-class-positive
           (acceptance-set (acceptance-set :class-ranges))
           (invert false))
         (production :character-class (#\[ #\^ :class-ranges #\]) character-class-negative
           (acceptance-set (acceptance-set :class-ranges))
           (invert true)))
       
       (rule :class-ranges ((acceptance-set (set character)))
         (production :class-ranges () class-ranges-none
           (acceptance-set (set-of character)))
         (production :class-ranges ((:nonempty-class-ranges dash)) class-ranges-some
           (acceptance-set (acceptance-set :nonempty-class-ranges))))
       
       (grammar-argument :delta dash no-dash)
       
       (rule (:nonempty-class-ranges :delta) ((acceptance-set (set character)))
         (production (:nonempty-class-ranges :delta) ((:class-atom dash)) nonempty-class-ranges-final
           (acceptance-set (acceptance-set :class-atom)))
         (production (:nonempty-class-ranges :delta) ((:class-atom :delta) (:nonempty-class-ranges no-dash)) nonempty-class-ranges-non-final
           (acceptance-set
            (character-set-union (acceptance-set :class-atom)
                                 (acceptance-set :nonempty-class-ranges))))
         (production (:nonempty-class-ranges :delta) ((:class-atom :delta) #\- (:class-atom dash) :class-ranges) nonempty-class-ranges-range
           (acceptance-set
            (let ((range (set character) (character-range (acceptance-set :class-atom 1)
                                                          (acceptance-set :class-atom 2))))
              (character-set-union range (acceptance-set :class-ranges)))))
         (production (:nonempty-class-ranges :delta) (:null-escape :class-ranges) nonempty-class-ranges-null-escape
           (acceptance-set (acceptance-set :class-ranges))))
       (%print-actions)
       
       (define (character-range (low (set character)) (high (set character))) (set character)
         (if (or (/= (character-set-length low) 1) (/= (character-set-length high) 1))
           (throw (oneof syntax-error))
           (let ((l character (character-set-min low))
                 (h character (character-set-min high)))
             (if (char<= l h)
               (set-of-ranges character l h)
               (throw (oneof syntax-error))))))
       
       
       (%subsection "Character Class Range Atoms")
       
       (rule (:class-atom :delta) ((acceptance-set (set character)))
         (production (:class-atom :delta) ((:class-character :delta)) class-atom-character
           (acceptance-set (set-of character ($default-action :class-character))))
         (production (:class-atom :delta) (#\\ :class-escape) class-atom-escape
           (acceptance-set (acceptance-set :class-escape))))
       
       (%charclass (:class-character dash))
       (%charclass (:class-character no-dash))
       
       (rule :class-escape ((acceptance-set (set character)))
         (production :class-escape (:decimal-escape) class-escape-decimal
           (acceptance-set
            (if (= (escape-value :decimal-escape) 0)
              (set-of character #?0000)
              (throw (oneof syntax-error)))))
         (production :class-escape (#\b) class-escape-backspace
           (acceptance-set (set-of character #?0008)))
         (production :class-escape (:character-escape) class-escape-character-escape
           (acceptance-set (set-of character (character-value :character-escape))))
         (production :class-escape (:character-class-escape) class-escape-character-class-escape
           (acceptance-set (acceptance-set :character-class-escape))))
       (%print-actions)
       )))
  
  (defparameter *rl* (world-lexer *rw* 'regexp-lexer))
  (defparameter *rg* (lexer-grammar *rl*)))


(defun run-regexp (regexp input &optional ignore-case multiline)
  (let ((exec (first (lexer-parse *rl* regexp))))
    (dotimes (i (length input) '(failure))
      (let ((result (funcall exec (list input ignore-case multiline) i)))
        (ecase (first result)
          (success
           (return (list* i (subseq input i (second result)) (cddr result))))
          (failure))))))

#|
(progn
  (depict-rtf-to-local-file
   "JS20/RegExpGrammar.rtf"
   "Regular Expression Grammar"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *rw* :visible-semantics nil)))
  (depict-rtf-to-local-file
   "JS20/RegExpSemantics.rtf"
   "Regular Expression Semantics"
   #'(lambda (rtf-stream)
       (depict-world-commands rtf-stream *rw*))))

(progn
  (depict-html-to-local-file
   "JS20/RegExpGrammar.html"
   "Regular Expression Grammar"
   t
   #'(lambda (html-stream)
       (depict-world-commands html-stream *rw* :visible-semantics nil))
   :external-link-base "notation.html")
  (depict-html-to-local-file
   "JS20/RegExpSemantics.html"
   "Regular Expression Semantics"
   t
   #'(lambda (html-stream)
       (depict-world-commands html-stream *rw*))
   :external-link-base "notation.html"))

(with-local-output (s "JS20/RegExpGrammar.txt") (print-lexer *rl* s) (print-grammar *rg* s))

(lexer-pparse *rl* "a+" :trace t)
(lexer-pparse *rl* "[]+" :trace t)
(run-regexp "(0x|0)2" "0x20")
(run-regexp "(a*)b\\1+c" "aabaaaac")
(run-regexp "(a*)b\\1+" "baaaac")
(run-regexp "b(a+)(a+)?(a+)c" "baaaac")
(run-regexp "(((a+)?(b+)?c)*)" "aacbbbcac")
(run-regexp "(\\s\\S\\s)" "aac xa d fds  fds sac")
(run-regexp "(\\s)" "aac xa deac")
(run-regexp "[01234]+aa+" "93-43aabbc")
(run-regexp "[\\101A-ae-]+" "93ABC-@ezy43abc")
(run-regexp "[\\181A-ae-]+" "93ABC-@ezy43abc")
(run-regexp "b[ace]+" "baaaacecfe")
(run-regexp "b[^a]+" "baaaabc")
(run-regexp "(?=(a+))a*b\\1" "baaabac")
(run-regexp "(?=(a+))" "baaabac")
(run-regexp "(.*?)a(?!(a+)b\\2c)\\2(.*)" "baaabaac")
(run-regexp "(aa|aabaac|ba|b|c)*" "aabaac")
(run-regexp "[\\Q^01234]+\\Qaa+" "93-43aabbc")
|#

#+allegro (clean-grammar *rg*) ;Remove this line if you wish to print the grammar's state tables.
(length (grammar-states *rg*))
