#include <stdio.h>

#include <ConditionalMacros.h>

int main(int argc, char* argv[])
{
    FILE* file = fopen("BuildSystemInfo.pm", "w");
    if (file != NULL) {
        fprintf(file, "$UNIVERSAL_INTERFACES_VERSION=0x%04X;\n", UNIVERSAL_INTERFACES_VERSION);
        fclose(file);
    }
}
