.. role:: bash(code)
   :language: bash

.. role:: js(code)
   :language: javascript

.. role:: python(code)
   :language: python


===========================
Fluent to Fluent Migrations
===========================

When migrating existing Fluent messages,
it's possible to copy a source directly with :python:`COPY_PATTERN`,
or to apply string replacements and other changes
by extending the :python:`TransformPattern` visitor class.

These transforms work with individual Fluent patterns,
i.e. the body of a Fluent message or one of its attributes.

Copying Fluent Patterns
-----------------------

Consider for example a patch modifying an existing message to move the original
value to a :js:`alt` attribute.

Original message:


.. code-block:: fluent

  about-logins-icon = Warning icon
      .title = Breached website


New message:


.. code-block:: fluent

  about-logins-breach-icon =
      .alt = Warning icon
      .title = Breached website


This type of changes requires a new message identifier, which in turn causes
existing translations to be lost. It’s possible to migrate the existing
translated content with:


.. code-block:: python

    from fluent.migrate import COPY_PATTERN

    ctx.add_transforms(
        "browser/browser/aboutLogins.ftl",
        "browser/browser/aboutLogins.ftl",
        transforms_from(
    """
    about-logins-breach-icon =
        .alt = {COPY_PATTERN(from_path, "about-logins-icon")}
        .title = {COPY_PATTERN(from_path, "about-logins-icon.title")}
    """,from_path="browser/browser/aboutLogins.ftl"),
    )


In this specific case, the destination and source files are the same. The dot
notation is used to access attributes: :js:`about-logins-icon.title` matches
the :js:`title` attribute of the message with identifier
:js:`about-logins-icon`, while :js:`about-logins-icon` alone matches the value
of the message.


.. warning::

  The second argument of :python:`COPY_PATTERN` and :python:`TransformPattern`
  identifies a pattern, so using the message identifier will not
  migrate the message as a whole, with all its attributes, only its value.

Transforming Fluent Patterns
----------------------------

To apply changes to Fluent messages, you may extend the
:python:`TransformPattern` class to create your transformation.
This is a powerful general-purpose tool, of which :python:`COPY_PATTERN` is the
simplest extension that applies no transformation to the source.

Consider for example a patch copying an existing message to strip out its HTML
content to use as an ARIA value.

Original message:


.. code-block:: fluent

  videocontrols-label =
      { $position }<span data-l10n-name="duration"> / { $duration }</span>


New message:


.. code-block:: fluent

  videocontrols-scrubber =
      .aria-valuetext = { $position } / { $duration }


A migration may be applied to create this new message with:


.. code-block:: python

    from fluent.migrate.transforms import TransformPattern
    import fluent.syntax.ast as FTL

    class STRIP_SPAN(TransformPattern):
        def visit_TextElement(self, node):
            node.value = re.sub("</?span[^>]*>", "", node.value)
            return node

    def migrate(ctx):
        path = "toolkit/toolkit/global/videocontrols.ftl"
        ctx.add_transforms(
            path,
            path,
            [
                FTL.Message(
                    id=FTL.Identifier("videocontrols-scrubber"),
                    attributes=[
                        FTL.Attribute(
                            id=FTL.Identifier("aria-valuetext"),
                            value=STRIP_SPAN(path, "videocontrols-label"),
                        ),
                    ],
                ),
            ],
        )


Note that a custom extension such as :python:`STRIP_SPAN` is not supported by
the :python:`transforms_from` utility, so the list of transforms needs to be
defined explicitly.

Internally, :python:`TransformPattern` extends the `fluent.syntax`__
:python:`Transformer`, which defines the :python:`FTL` AST used here.
As a specific convenience, pattern element visitors such as
:python:`visit_TextElement` are allowed to return a :python:`FTL.Pattern`
to replace themselves with more than one node.

__ https://projectfluent.org/python-fluent/fluent.syntax/stable/
