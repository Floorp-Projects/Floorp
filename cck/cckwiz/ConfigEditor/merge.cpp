
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream.h>
#include <fstream.h>
#include <windows.h>
#include <ctype.h>
//#include <globalheader.h>


typedef struct
{
	int width;
	int height;
} DIMENSION;	

typedef struct 
{
	char name[50];
	char value[50];
	char type[20];
	DIMENSION size;
	POINT location;
	char options[20];
} widget;

widget ptr_ga[1000];

int isnum(char valuestring[50]);

int isnum(char valuestring[50])
{
	//cout << "this is the value string " << valuestring << "\n";
	for (int i=0; i < (strlen(valuestring)); i++)
	{	if(!isdigit(valuestring[i]))
	{//cout << "this is the string char " <<valuestring[i] <<"\n";
		return 0;
	}
	}
	return 1;
}

char *GetGlobal (char *fname);

char *GetGlobal (char *fname)
{
	

	for (int i=0;i<1000;i++)
	{

		if (strcmp (fname, ptr_ga[i].name) == 0)
			return (ptr_ga[i].value);	
			
	} 

    cout << ("error:variable not found \n");

	return NULL;
}




int main(int argc, char *argv[])
{
int i = 0;

ifstream myin("test.dat");
ifstream prefin("pref.dat");
ifstream addition("addition.js");
ofstream myout("out.js");

if(!myin) {
	cout << "cannot open the file \n";
	return 1;
}


while (!myin.eof()) {
	myin >> ptr_ga[i].name >> ptr_ga[i].value ;
//	cout << ptr_ga[i].name <<","<< ptr_ga[i].value <<"\n";

	i++;

}

myin.close();
if(!myout) {
	cout << "cannot open the file \n";
	return 1;
}

if (argc == 1) 
{
	
	char prefer [7];
	char prefname[50];
	char pref1[5];
	char pref2[7];
	char bool1[5];
	char bool2[6];

if(!prefin) {
	cout << "cannot open the file \n";
	return 1;
}


while (!prefin.eof()) {

	prefin >> prefer >> prefname ;
//	cout <<"This is "<< prefer << "and " << prefname << "\n";
	i++;



	strcpy(pref1, "pref");
	strcpy(pref2, "config");
	strcpy(bool1, "true");
	strcpy(bool2, "false");

	if (strcmp(prefer,pref1) ==0)
	{
//		cout << "inside the def pref \n";
		if (GetGlobal(prefname)!= NULL)
		{	if (( strcmp (GetGlobal(prefname), bool1) == 0)|| ( strcmp (GetGlobal(prefname), bool2)== 0) || (isnum (GetGlobal(prefname))))
		{	//cout << "the current value is " <<GetGlobal(prefname)<<"\n";
			myout<< "defaultPref(\"" << prefname << "\", " <<GetGlobal(prefname) <<");\n";
		}
			else 
			myout<< "defaultPref(\"" << prefname << "\", \"" <<GetGlobal(prefname) <<"\");\n";
		}
		else
			cout << prefname << " is not found\n";
	}
	
	else if (strcmp(prefer,pref2) ==0)
	{
//		cout << "inside the config \n";

		if (GetGlobal(prefname)!= NULL)
		{	if (( strcmp (GetGlobal(prefname), bool1) == 0)|| ( strcmp (GetGlobal(prefname), bool2) == 0) || (isnum (GetGlobal(prefname))))
		{//cout << "the value of isnum is " << isnum <<"\n";
		//cout << "the curretn value is "<<GetGlobal(prefname)<<"\n";
		myout<< "config(\"" << prefname << "\", " <<GetGlobal(prefname) <<");\n";
		}
		else 
			myout<< "config(\"" << prefname << "\", \"" <<GetGlobal(prefname) <<"\");\n";
		}
		else
			cout << prefname << " is not found\n";
	}



}

}




if(!addition) {
	cout << "cannot open the file \n";
	return 1;
}


while (!addition.eof()) {

	char jsprefname[150];
	
	addition.getline(jsprefname,150);
	char *quote_ptr1;
	char *quote_ptr2;
	quote_ptr1 = strchr(jsprefname, '"');
	quote_ptr2 = strchr((quote_ptr1+1), '"');
	char jspref[100];
	strncpy(jspref, (quote_ptr1 +1),(quote_ptr2-quote_ptr1-1));
	jspref[(quote_ptr2-quote_ptr1-1)] = NULL;
//	printf("%s \n", jsprefname);
//	printf("%s \n", jspref);
//	printf("%s \n", (quote_ptr1 +1));
//	printf("%s \n", (quote_ptr2 +1));
		if (GetGlobal(jspref)!= NULL)
			//cout << "The preference \"" << jspref << "\" already exists.\n";
		{	printf("the preference ");
			printf("%s", jspref);	
			printf("already exists.\n");}
	myout << jsprefname <<"\n";		 
}	
myout.close();
addition.close();
return 1;
}
