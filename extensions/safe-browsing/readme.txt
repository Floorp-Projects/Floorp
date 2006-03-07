To enable:

  1. Add "ac_add_options --enable-extensions=default,safe-browsing" to your
     .mozconfig file
  2. Set the preference "extensions.safebrowsing.enabled" to true

Source organization:

  - src/ contains our component (the loader)
  - content/ contains xul and whatnot
  - locale/ contains our strings
  - lib/ contains our application files
  - lib/js contains pure js library files
  - lib/moz contains Mozilla-specific library files
