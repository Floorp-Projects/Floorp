``properties_iter::ffi``
========================

.. js:class:: CodePointRangeIterator

    An iterator over code point ranges, produced by ``ICU4XCodePointSetData`` or one of the ``ICU4XCodePointMapData`` types


    .. js:method:: next()

        Advance the iterator by one and return the next range.

        If the iterator is out of items, ``done`` will be true


.. js:class:: CodePointRangeIteratorResult

    Result of a single iteration of :js:class:`CodePointRangeIterator`. Logically can be considered to be an ``Option<RangeInclusive<u32>>``,

    ``start`` and ``end`` represent an inclusive range of code points start, end, and ``done`` will be true if the iterator has already finished. The last contentful iteration will NOT produce a range done=true, in other words ``start`` and ``end`` are useful values if and only if ``done=false``.


    .. js:attribute:: start

    .. js:attribute:: end

    .. js:attribute:: done
