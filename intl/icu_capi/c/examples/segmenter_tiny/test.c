// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#include "../../include/ICU4XLineSegmenter.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    ICU4XDataProvider* provider = ICU4XDataProvider_create_compiled();

    diplomat_result_box_ICU4XLineSegmenter_ICU4XError segmenter_result = ICU4XLineSegmenter_create_auto(provider);
    if (!segmenter_result.is_ok)  {
        printf("Failed to create ICU4XLineSegmenter\n");
        return 1;
    }
    ICU4XLineSegmenter* segmenter = segmenter_result.ok;

    char output[40];
    DiplomatWriteable write = diplomat_simple_writeable(output, 40);

    const char* data = "อักษรไทย เป็นอักษรที่ใช้เขียนภาษาไทยและภาษาของกลุ่มชาติพันธุ์ต่างๆ เช่น คำเมือง, อีสาน, ภาษาไทยใต้, มลายูปัตตานี เป็นต้น ในประเทศไทย มีพยัญชนะ 44 รูป สระ 21 รูป วรรณยุกต์ 4 รูป และเครื่องหมายอื่น ๆ อีกจำนวนหนึ่ง";

    ICU4XLineBreakIteratorUtf8* iter = ICU4XLineSegmenter_segment_utf8(segmenter, data, strlen(data));

    printf("Breakpoints:");
    while (true) {
        int32_t breakpoint = ICU4XLineBreakIteratorUtf8_next(iter);
        if (breakpoint == -1) {
            break;
        }
        printf(" %d", breakpoint);
    }

    printf("\n");

    ICU4XLineBreakIteratorUtf8_destroy(iter);
    ICU4XLineSegmenter_destroy(segmenter);
    ICU4XDataProvider_destroy(provider);

    return 0;
}
