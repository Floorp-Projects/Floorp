#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <cctype>

#include "qcms.h"

using std::isspace;

/* Nabbed from the http://www.jonh.net/~jonh/md5/crc32/crc32.c. License is
 * "do anything, no restrictions." */
unsigned long crc32(const unsigned char *s, unsigned int len);

/*
 * Test Framework Header stuff
 */

#define ASSERT(foo) { \
    if (!(foo)) { \
        fprintf(stderr, "%s: Failed Assertion Line %d\n", __FILE__, __LINE__); \
        exit(-1); \
    } \
}

#define CHECK(condition, var, value, message, label) { \
    if (!(condition)) { \
        var = value; \
        fprintf(stderr, message); \
        goto label; \
    } \
}

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) <= (b) ? (a) : (b))
#define ABS(a) ((a < 0) ? -a : a)

#define BITMAP_PIXEL_COUNT (256 * 256 * 256)
#define BITMAP_SIZE (BITMAP_PIXEL_COUNT * 3)

/* Relative, Perceptual, and Saturation all take the same code path through
 * LCMS. As such, we just check perceptual and absolute. */
int testedIntents[] = {INTENT_PERCEPTUAL, INTENT_ABSOLUTE_COLORIMETRIC};
#define TESTED_INTENT_COUNT (sizeof(testedIntents)/sizeof(int))

const char *profileDir = "testprofiles";

/* Parameters detailing a single test. */
struct TestParams {

    /* name of the input profile. */
    char *iProfileName;

    /* name of the output profile. */
    char *oProfileName;

    /* Golden CRC32s. */
    unsigned goldenCRCs[TESTED_INTENT_COUNT];

    /* Did we read golden sums? */
    int hasGolden;

    /* Generated CRC32. */
    unsigned ourCRCs[TESTED_INTENT_COUNT];

    /* Linked list pointer. */
    struct TestParams *next;

};

/* Top level context structure for the test run. */
struct TestContext {

    /* Base path for files. */
    char *basePath;

    /* Linked list of param structures. */
    struct TestParams *paramList;

    /* Our GIANT, ~50 meg buffers for every pixel value. */
    unsigned char *src, *fixedX, *floatX;

};

/* Reads a line from the directive file. Returns 0 on success, 
   -1 on malformed file. */
static int 
ReadTestFileLine(struct TestContext *ctx, FILE *handle) 
{

    /* Locals. */
    char buff[4096];
    int status = 0;
    char *iter, *base;
    char *strings[2];
    char *rv;
    unsigned i;
    struct TestParams *params = NULL;
   
    /* Check input. */
    ASSERT(ctx != NULL);
    ASSERT(handle != NULL);

    /* Null out string pointers. */
    for (i = 0; i < 2; ++i)
        strings[i] = NULL;

    /* Read in the line. */
    rv = fgets(buff, 4096, handle);
    if (feof(handle))
        goto done;
    CHECK(rv != NULL, status, -1, "Bad Test File\n", error);

    /* Allow for comments and blanklines. */
    if ((buff[0] == '#') || isspace(buff[0]))
        goto done;

    /* Allocate a param file. */
    params = (struct TestParams *) calloc(sizeof(struct TestParams), 1);
    ASSERT(params);

    /* Parse the profile names. */
    iter = buff;
    for (i = 0; i < 2; ++i) {
        for (base = iter; (*iter != '\0') && !isspace(*iter); ++iter);
        *iter = '\0';
        CHECK((iter - base) > 0, status, -1, "Bad Test File\n", error);
        strings[i] = strdup(base);
        ++iter;
    }

    /* Fill the param file. */
    params->iProfileName = strings[0];
    params->oProfileName = strings[1];

    /* Skip any whitespace. */
    for (; (*iter != '\0') && isspace(*iter); ++iter);

    /* if we have more to parse, we should have golden CRCs. */
    if (*iter != '\0') {
        for (i = 0; i < TESTED_INTENT_COUNT; ++i) {
            params->goldenCRCs[i] = strtoul(iter, &base, 16);
            CHECK((errno != EINVAL) && (errno != ERANGE) && (base != iter),
                  status, -1, "Bad Checksum List\n", error);
            iter = base;
        }
        params->hasGolden = 1;
    }

    /* Link up our param structure. */
    params->next = ctx->paramList;
    ctx->paramList = params;

    done:
    return status;

    error:

    /* Free the strings. */
    for (i = 0; i < 2; ++i)
        free(strings[i]);

    /* Free the param structure. */
    if (params != NULL)
        free(params);
    
    return status;
}

/* Initializes the test context. 0 on success, -1 on failure. */
static int 
TestInit(struct TestContext *ctx, const char *filePath)
{

    /* Locals. */
    FILE *tfHandle = NULL;
    const char *iter, *last;
    unsigned n;
    int status = 0;
    unsigned i, j, k, l;
    struct TestParams *curr, *before, *after;

    /* Checks. */
    ASSERT(ctx != NULL);
    ASSERT(filePath != NULL);

    /* Allocate our buffers. If it's going to fail, we should know now. */
    ctx->src = (unsigned char *) malloc(BITMAP_SIZE);
    CHECK(ctx->src != NULL, status, -1, "Can't allocate enough memory\n", error);
    ctx->fixedX = (unsigned char *) malloc(BITMAP_SIZE);
    CHECK(ctx->fixedX != NULL, status, -1, "Can't allocate enough memory\n", error);
    ctx->floatX = (unsigned char *) malloc(BITMAP_SIZE);
    CHECK(ctx->floatX != NULL, status, -1, "Can't allocate enough memory\n", error);

    /* Open the test file. */
    tfHandle = fopen(filePath, "r");
    CHECK(tfHandle != NULL, status, -1, "Unable to open test file\n", done);

    /* Extract the base. XXX: Do we need to worry about windows separators? */
    for (last = iter = filePath; *iter != '\0'; ++iter)
        if (*iter == '/')
            last = iter;
    n = last - filePath;
    ctx->basePath = (char *) malloc(n + 1);
    ASSERT(ctx->basePath != NULL);
    memcpy(ctx->basePath, filePath, n);
    ctx->basePath[n] = '\0';

    /* Read through the directive file. */
    while (!feof(tfHandle)) {
        CHECK(!ReadTestFileLine(ctx, tfHandle), status, -1, 
              "Failed to Read Test File\n", error);
    }

    /* Reverse the list so that we process things in the order we read them
       in. */
    curr = ctx->paramList;
    before = NULL;
    while (curr->next != NULL) {
        after = curr->next;
        curr->next = before;
        before = curr;
        curr = after;
    }
    curr->next = before;
    ctx->paramList = curr;

    /* Generate our source bitmap. */
    printf("Generating source bitmap...");
    fflush(stdout);
    for (i = 0; i < 256; ++i) {
        for (j = 0; j < 256; ++j)
            for (k = 0; k < 256; ++k) {
                l = ((256 * 256 * i) + (256 * j) + k) * 3;
                ctx->src[l] = (unsigned char) i;
                ctx->src[l + 1] = (unsigned char) j;
                ctx->src[l + 2] = (unsigned char) k;
            }
    }
    ASSERT(l == (BITMAP_SIZE - 3));
    printf("done!\n");
    
    goto done;

    error:
    /* Free up the buffers. */
    if (ctx->src != NULL)
        free(ctx->src);
    if (ctx->fixedX != NULL)
        free(ctx->fixedX);
    if (ctx->floatX != NULL)
        free(ctx->floatX);

    done:

    /* We're done with the test directive file. */
    if (tfHandle != NULL)
        fclose(tfHandle);

    return status;
}

/* Runs a test for the given param structure. Returns 0 on success (even if
 * the test itself fails), -1 on code failure.
 *
 * 'mode' is either "generate" or "check".
 *
 * 'intentIndex' is an index in testedIntents
 */
static int 
RunTest(struct TestContext *ctx, struct TestParams *params, 
        char *mode, unsigned intentIndex) 
{

    /* Locals. */
    cmsHPROFILE inProfile = NULL;
    cmsHPROFILE outProfile = NULL;
    cmsHTRANSFORM transformFixed = NULL;
    cmsHTRANSFORM transformFloat = NULL;
    char *filePath;
    unsigned i;
    int difference;
    int failures;
    int status = 0;

    /* Allocate a big enough string for either file path. */
    filePath = (char *)malloc(strlen(ctx->basePath) + 1 +
                              strlen(profileDir) + 1 +
                              MAX(strlen(params->iProfileName),
                                  strlen(params->oProfileName)) + 1);
    ASSERT(filePath != NULL);

    /* Set up the profile path for the input profile. */
    strcpy(filePath, ctx->basePath);
    strcat(filePath, "/");
    strcat(filePath, profileDir);
    strcat(filePath, "/");
    strcat(filePath, params->iProfileName);
    inProfile = cmsOpenProfileFromFile(filePath, "r");
    CHECK(inProfile != NULL, status, -1, "unable to open input profile!\n", done);

    /* Set up the profile path for the output profile. */
    strcpy(filePath, ctx->basePath);
    strcat(filePath, "/");
    strcat(filePath, profileDir);
    strcat(filePath, "/");
    strcat(filePath, params->oProfileName);
    outProfile = cmsOpenProfileFromFile(filePath, "r");
    CHECK(outProfile != NULL, status, -1, "unable to open input profile!\n", done);

    /* Precache. */
    cmsPrecacheProfile(inProfile, CMS_PRECACHE_LI16W_FORWARD);
    cmsPrecacheProfile(inProfile, CMS_PRECACHE_LI8F_FORWARD);
    cmsPrecacheProfile(outProfile, CMS_PRECACHE_LI1616_REVERSE);
    cmsPrecacheProfile(outProfile, CMS_PRECACHE_LI168_REVERSE);

    /* Create the fixed transform. */
    transformFixed = cmsCreateTransform(inProfile, TYPE_RGB_8, 
                                        outProfile, TYPE_RGB_8, 
                                        testedIntents[intentIndex], 0);
    CHECK(transformFixed != NULL, status, -1, 
          "unable to create fixed transform!\n", done);

    /* Do the fixed transform. */
    cmsDoTransform(transformFixed, ctx->src, ctx->fixedX, BITMAP_PIXEL_COUNT);

    /* Compute the CRC of the fixed transform. */
    params->ourCRCs[intentIndex] = crc32(ctx->fixedX, BITMAP_SIZE);

    /* If we're just generating, we have everything we need. */
    if (!strcmp(mode, "generate")) {
        printf("In: %s, Out: %s, Intent: %u Generated\n",
               params->iProfileName, params->oProfileName, testedIntents[intentIndex]);
        goto done;
    }

    /* Create the float transform. */
    transformFloat = cmsCreateTransform(inProfile, TYPE_RGB_8, 
                                        outProfile, TYPE_RGB_8, 
                                        testedIntents[intentIndex], 
                                        cmsFLAGS_FLOATSHAPER);
    CHECK(transformFloat != NULL, status, -1, 
          "unable to create float transform!\n", done);

    /* Make sure we have golden values. */
    CHECK(params->hasGolden, status, -1, 
          "Error: Check mode enabled but no golden values in file\n", done);

    /* Print out header. */
    printf("In: %s, Out: %s, Intent: %u\n", 
           params->iProfileName, params->oProfileName, 
           testedIntents[intentIndex]);

    /* CRC check the fixed point path. */
    if (params->goldenCRCs[intentIndex] == params->ourCRCs[intentIndex])
        printf("\tPASSED - CRC Check of Fixed Point Path\n");
    else
        printf("\tFAILED - CRC Check of Fixed Point Path - Expected %x, Got %x\n", 
               params->goldenCRCs[intentIndex], params->ourCRCs[intentIndex]);

    /* Do the floating point transform. */
    cmsDoTransform(transformFloat, ctx->src, ctx->floatX, BITMAP_PIXEL_COUNT);

    /* Compare fixed with floating. */
    failures = 0;
    for (i = 0; i < BITMAP_SIZE; ++i) {
        difference = (int)ctx->fixedX[i] - (int)ctx->floatX[i];
        /* Allow off-by-one from fixed point, nothing more. */
        if (ABS(difference) > 1)
            ++failures;
    }
    if (failures == 0)
        printf("\tPASSED - floating point path within acceptable parameters\n");
    else
        printf("\tWARNING - floating point path off by 2 or more in %d cases!\n",
               failures);

    done:

    /* Free the temporary string. */
    free(filePath);

    /* Close the transforms and profiles if non-null. */
    if (transformFixed != NULL)
        cmsDeleteTransform(transformFixed);
    if (transformFloat != NULL)
        cmsDeleteTransform(transformFloat);
    if (inProfile != NULL)
        cmsCloseProfile(inProfile);
    if (outProfile != NULL)
        cmsCloseProfile(outProfile);

    return status;
}

/* Writes the in memory data structures out to the original test directive
 * file, using the generated CRCs as golden CRCs. */
static int 
WriteTestFile(struct TestContext *ctx, const char *filename) 
{

    /* Locals. */
    FILE *tfHandle = NULL;
    int status = 0;
    struct TestParams *iter;
    unsigned i;

    /* Check Input. */
    ASSERT(ctx != NULL);
    ASSERT(filename != NULL);

    /* Open the file in write mode. */
    tfHandle = fopen(filename, "w");
    CHECK(tfHandle != NULL, status, -1, "Couldn't Open Test File For Writing",
          done);

    /* Print Instructional Comment. */
    fprintf(tfHandle, "# Color Management Test Directive File\n#\n# Format:\n"
                      "# InputProfileFilename OutputProfileFilename "
                      "<CRC32 For Each Intent>\n#\n");
    /* Iterate and Print. */
    for (iter = ctx->paramList; iter != NULL; iter = iter->next) {
        fprintf(tfHandle, "%s %s", iter->iProfileName, iter->oProfileName);
        for (i = 0; i < TESTED_INTENT_COUNT; ++i)
            fprintf(tfHandle, " %x", iter->ourCRCs[i]);
        fprintf(tfHandle, "\n");
    }

    done:

    /* Close the test file. */
    if (tfHandle != NULL)
        fclose(tfHandle);

    return status;
}


/* Main Function. */
int 
main (int argc, char **argv) 
{

    /* Locals. */
    struct TestContext ctx;
    struct TestParams *iter;
    unsigned i;
    int status = 0;

    /* Zero out context. */
    memset(&ctx, 0, sizeof(ctx));

    if ((argc != 3) || 
        (strcmp(argv[1], "generate") && strcmp(argv[1], "check"))) {
        printf("Usage: %s generate|check PATH/FILE.cmtest\n", argv[0]);
        return -1;
    }

    /* Initialize the test. */
    TestInit(&ctx, argv[2]);

    /* Run each individual test. */
    iter = ctx.paramList;
    while (iter != NULL) {

        /* For each intent. */
        for (i = 0; i < TESTED_INTENT_COUNT; ++i)
            CHECK(!RunTest(&ctx, iter, argv[1], i),
                  status, -1, "RunTest Failed\n", done);
        iter = iter->next;
    }

    /* If we're generating, write back out. */
    if (!strcmp(argv[1], "generate"))
        WriteTestFile(&ctx, argv[2]);

    done:

    return status;
}


/*
 * CRC32 Implementation.
 */

static unsigned long crc32_tab[] = {
      0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
      0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
      0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
      0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
      0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
      0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
      0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
      0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
      0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
      0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
      0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
      0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
      0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
      0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
      0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
      0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
      0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
      0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
      0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
      0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
      0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
      0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
      0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
      0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
      0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
      0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
      0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
      0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
      0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
      0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
      0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
      0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
      0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
      0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
      0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
      0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
      0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
      0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
      0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
      0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
      0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
      0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
      0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
      0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
      0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
      0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
      0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
      0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
      0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
      0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
      0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
      0x2d02ef8dL
   };

/* Return a 32-bit CRC of the contents of the buffer. */

unsigned long 
crc32(const unsigned char *s, unsigned int len)
{
  unsigned int i;
  unsigned long crc32val;
  
  crc32val = 0;
  for (i = 0;  i < len;  i ++)
    {
      crc32val =
	crc32_tab[(crc32val ^ s[i]) & 0xff] ^
	  (crc32val >> 8);
    }
  return crc32val;
}

