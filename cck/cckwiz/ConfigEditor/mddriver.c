
//#define MD 5

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
//#include "global.h"
//#include "md5.h"
//#include "md5c.c"
//#include "nsMsgMD5.h"


//#define MD5_LENGTH 16
#define OBSCURE_CODE 7
const void *nsMsgMD5Digest(const void *msg, unsigned int len);

static void MDString (unsigned char *, char *);
//static void MDFile   (unsigned char *, char *);
static void MDPrint  (char *, char *, unsigned char *, long);
void obscure (const char *, char *, int);

//#define MD_CTX MD5_CTX
//#define MDInit MD5Init
//#define MDUpdate MD5Update
//#define MDFinal MD5Final

// Main driver.

 

short bflag = 1;	/* 1 == print sums in binary */

int main (argc, argv)
int argc;
char *argv[];
{
   char outputfile[] = "netscape.cfg";
  unsigned char* digest;//[MD5_LENGTH];
  long f_size=0;
  int index=0;
  int num=0;
  char *file_buffer;
  char *hash_input;
  char final_buf[50];
  char final_hash[49];
  char *magic_key = "VonGloda5652TX75235ISBN";
  unsigned int key_len  =(strlen (magic_key)+1);
  FILE *outp;
  FILE *input_file;
  unsigned int len_buffer;

  printf ("before opening the file \n");

	 if ((input_file = fopen (argv[1], "rb")) == NULL){
	  printf ("%s can't be opened for reading\n", argv[1]);
		 } else {  printf ("after opening the file \n");

		fseek(input_file, 0,2);

		f_size = ftell(input_file);

		fseek (input_file,0,0);

		file_buffer = (char *) malloc (f_size);
		hash_input = (char *) malloc (f_size +key_len);

		fread (file_buffer,1,f_size,input_file);

		file_buffer[f_size]=NULL;
  printf ("%s is the statement \n", magic_key);

		strcpy (hash_input , file_buffer);
  printf ("%s is 2 hash input statement \n",hash_input);
	//	printf ("%s\n",file_buffer);
//	strncat (hash_input,magic_key,key_len);
//  printf ("%s is 1 hash input statement \n",hash_input);
//  printf ("%d is the length \n", strlen(hash_input));
  hash_input[strlen(hash_input)]=NULL;

		 }
  if (argc > 1) {
		//	MDFile (digest,argv[1]);
//	  	MDString (digest, file_buffer);
	  digest = (unsigned char *)nsMsgMD5Digest(hash_input, strlen(hash_input));
printf("%s is the digest \n", digest);
	  for (index =0; index <16;++index)
			{
				strcpy(&(final_hash[3*index])," ");
				num=digest[index];
			//	printf("the num is %d and the dig is %s\n", num,&(digest[index]));
				sprintf(&(final_hash[(3*index)+1]),"%0.2x",num);
			//	printf ("inside the for %s and the index %d \n", &(final_hash[3*index]), index);
			}
		final_hash[48]=NULL;
	//	printf("the hashed output is %s\n", final_hash);
		strncpy (final_buf, "//",2);
		final_buf[2]=NULL;
	//	printf ("the final hex %0.2x \n", "b");
		strncat(final_buf,final_hash,48);
	//	printf ("the final buf %s\n",final_buf);
		final_buf[50]=NULL;
printf ("%s is the final buffer \n",final_buf);
	  MDPrint (outputfile, file_buffer, final_buf,f_size);
  } else {

    printf("Usage: md5 <file> \n");
  }
//free(file_buffer);
  return (0);
}

// To convert to Hex String
/*void HexConvert(digest, final_hash)


  
{
char *tuple;
 char *map ="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
char *output = final_hash;
int index=0;
for (index =0; index <16;++index)
	{
		char *tuple =map[digest[index]];
		*output++ = *tuple++;
		*output++ = *tuple++;
	}
*output ='\0';
}
*/


	// Digests a file and prints the result.

/*static void MDFile (digest, filename)
unsigned char *digest;
char *filename;
{
  FILE *file;
  MD_CTX context;
  int len;
  unsigned char buffer[1024];
  unsigned char magic_key[] = "VonGloda5652TX75235ISBN\0";
  unsigned int key_len  =strlen (magic_key);
  if ((file = fopen (filename, "rb")) == NULL)
    printf ("%s can't be opened\n", filename);
  else {
    MDInit (&context);

		MDUpdate (&context, magic_key, key_len);
    while (len = fread (buffer, 1, 1024, file))
		 MDUpdate (&context, buffer, len);
    MDFinal (digest, &context);

    fclose (file);
  }
}
*/
// Digests a string and prints the result.
/* 
static void MDString (digest, str)
unsigned char *digest;
char *str;
{
  MD_CTX context;
  unsigned int len = strlen (str);
  unsigned char *magic_key = "VonGloda5652TX75235ISBN";
  unsigned int key_len  =(strlen (magic_key)+1);
  MDInit (&context);
  MDUpdate (&context, magic_key, key_len);
  MDUpdate (&context, str, len);
  MDFinal (digest, &context);

}
*/
void obscure (input, obscured, len) 
const char *input;
char *obscured;
int len;
{
	int i;
	for (i = 0; i < len; i++) {
		obscured[i] =  (input[i] + OBSCURE_CODE) ; 
	}
	obscured[len] = '\0';
}


/* Prints a message digest in hexadecimal or binary.
 */
static void MDPrint (outfile, file_buffer, final_buf, f_size)
char *outfile;
char *file_buffer;
unsigned char *final_buf;
//long file_size;
{
  FILE *outp;
  int len;
  unsigned char buffer[1024];
  char obscured[2000];
//printf("inside the mdprint \n");
  if ((outp = fopen (outfile, "wb")) == NULL) {
    printf ("%s can't be opened for writing\n", outfile);
  } else {
	if (bflag) {

		// print in obscured digest 
		obscure(final_buf, obscured, 50);
		printf ("finished first obscure\n");
		fprintf(outp, "%s", obscured);
		printf("%s is the 1 obscured \n",obscured);
		// print in obscured end of file 
		obscure("\n", obscured, 1);
		fprintf(outp, "%s", obscured);
		printf("%s is the 2 obscured \n",obscured);

		//print in obscured file
		obscure(file_buffer, obscured, f_size);
		fprintf(outp, "%s",obscured);
//		printf ("the digest length is %ld now \n",strlen(file_buffer));
		printf("%s is the 3 obscured \n",obscured);

	} else {/*
	
		// print in hex 
		obscure(digest, obscured, MD5_LENGTH);
		fprintf(outp, "%s\n", obscured);
		// for (i = 0; i < MD5_LENGTH; i++) {
		//  fprintf (outp, "%02x ", digest[i]);
		// } 
		//
		
		// print in obscured digest 
		obscure("\n", obscured, 1);
		fprintf(outp, "%s\n", obscured);

		while(len = fread (buffer, 1, 1024, inpp)) {
			obscure(buffer, obscured, 1024);
			fprintf(outp, "%s", obscured);
		}*/
		
	}

	fclose (outp);
//	fclose (inpp);
  }	
}
