# Graphite engine

## What is Graphite?

Graphite is a system that can be used to create “smart fonts” capable of displaying writing systems with various complex behaviors. A smart font contains not only letter shapes but also additional instructions indicating how to combine and position the letters in complex ways.

Graphite was primarily developed to provide the flexibility needed for minority languages which often need to be written according to slightly different rules than well-known languages that use the same script.

Examples of complex script behaviors Graphite can handle include:

* contextual shaping
* ligatures
* reordering
* split glyphs
* bidirectionality
* stacking diacritics
* complex positioning
* shape aware kerning
* automatic diacritic collision avoidance

See [examples of scripts with complex rendering](http://scripts.sil.org/CmplxRndExamples).

## Graphite system overview
The Graphite system consists of:

* A rule-based programming language [Graphite Description Language](http://scripts.sil.org/cms/scripts/page.php?site_id=projects&item_id=graphite_devFont#gdl) (GDL) that can be used to describe the behavior of a writing system
* A compiler for that language
* A rendering engine that can serve as the layout component of a text-processing application

Graphite renders TrueType fonts that have been extended by means of compiling a GDL program.

Further technical information is available on the [Graphite technical overview](http://scripts.sil.org/cms/scripts/page.php?site_id=projects&item_id=graphite_techAbout) page.
