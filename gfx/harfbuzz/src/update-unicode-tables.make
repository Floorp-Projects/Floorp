#!/usr/bin/env -S make -f

all: packtab arabic-table emoji-table indic-table tag-table ucd-table use-table vowel-constraints

.PHONY: all clean packtab arabic-table emoji-table indic-table tag-table ucd-table use-table vowel-constraints

arabic-table: gen-arabic-table.py ArabicShaping.txt UnicodeData.txt Blocks.txt
	./$^ > hb-ot-shape-complex-arabic-table.hh || (rm hb-ot-shape-complex-arabic-table.hh; false)
emoji-table: gen-emoji-table.py emoji-data.txt
	./$^ > hb-unicode-emoji-table.hh || (rm hb-unicode-emoji-table.hh; false)
indic-table: gen-indic-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt Blocks.txt
	./$^ > hb-ot-shape-complex-indic-table.cc || (rm hb-ot-shape-complex-indic-table.cc; false)
tag-table: gen-tag-table.py languagetags language-subtag-registry
	./$^ > hb-ot-tag-table.hh || (rm hb-ot-tag-table.hh; false)
ucd-table: gen-ucd-table.py ucd.nounihan.grouped.zip hb-common.h
	./$^ > hb-ucd-table.hh || (rm hb-ucd-table.hh; false)
use-table: gen-use-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt UnicodeData.txt Blocks.txt
	./$^ > hb-ot-shape-complex-use-table.cc || (rm hb-ot-shape-complex-use-table.cc; false)
vowel-constraints: gen-vowel-constraints.py ms-use/IndicShapingInvalidCluster.txt Scripts.txt
	./$^ > hb-ot-shape-complex-vowel-constraints.cc || (hb-ot-shape-complex-vowel-constraints.cc; false)

packtab:
	/usr/bin/env python3 -c "import packTab" 2>/dev/null || /usr/bin/env pip3 install git+https://github.com/harfbuzz/packtab

ArabicShaping.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/ArabicShaping.txt

UnicodeData.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/UnicodeData.txt

Blocks.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/Blocks.txt

emoji-data.txt:
	curl -O https://www.unicode.org/Public/UCD/latest/ucd/emoji/emoji-data.txt

IndicSyllabicCategory.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/IndicSyllabicCategory.txt

IndicPositionalCategory.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/IndicPositionalCategory.txt

languagetags:
	curl -O https://docs.microsoft.com/en-us/typography/opentype/spec/languagetags

language-subtag-registry:
	curl -O https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry

ucd.nounihan.grouped.zip:
	curl -O https://unicode.org/Public/UCD/latest/ucdxml/ucd.nounihan.grouped.zip

Scripts.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/Scripts.txt

clean:
	$(RM) \
		ArabicShaping.txt UnicodeData.txt Blocks.txt emoji-data.txt \
		IndicSyllabicCategory.txt IndicPositionalCategory.txt \
		languagetags language-subtag-registry ucd.nounihan.grouped.zip Scripts.txt
