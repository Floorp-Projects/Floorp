/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//Designed specifically for pulling out command line arugments
//used in the command line startup routine.  This class knows 
//about two kinds of delimeters, spaces and quotes.  Quotes
//when placed in front and in back of characters delimit spaces and
//any combination of any other characters.  Spaces delimit quoted
//strings and other non quoted strings.  Error checking is flaged
//on quoted string groups and non quoted string  which are 
//not separated by a space.  There is no failure continue mode.
//When an error is flaged, the routine exits and signals false.

//Command line parsing implementation file.     
#include "stdafx.h"
#include "cmdparse.h"
#include <ctype.h>


/************************************************************************/
//Purpose:  Contructor takes two arguments to initialize the class.
//Args:		pszStringToParse:  a pointer to the command line to parse
//			lStringList:	A reference to the list to hold the arguments
/************************************************************************/

CCmdParse::CCmdParse(char *pszStringToParse,CStringList &lStringList)
{
	strcpy(m_szParseString,pszStringToParse);
	m_pStringList = &lStringList;
	m_nIndex = 0;
}

CCmdParse::CCmdParse()
{
	m_pStringList = NULL;
	m_nIndex = 0;

}
/************************************************************************/
//Purpose:  Destructor removes any objects still on the list
/************************************************************************/
                                                                           
CCmdParse::~CCmdParse()
{
	if(m_pStringList)
	{                       
		POSITION pos1,pos2;
	  	char *pTemp = NULL;
	  	if (m_pStringList->GetCount() > 0)
		{
			for( pos1 = m_pStringList->GetHeadPosition(); ( pos2 = pos1 ) != NULL; )
			{
				m_pStringList->GetNext( pos1 );
																							 
				// Save the old pointer for
				//deletion.
				m_pStringList->RemoveAt( pos2 );
				 
			}
		}
	}
}

/************************************************************************/
//Purpose:  Initialize class members and construction has taken place
/************************************************************************/
void CCmdParse::Init(char *pszStringToParse,CStringList &lStringList)
{
	strcpy(m_szParseString,pszStringToParse);
	m_pStringList = &lStringList;
	m_nIndex = 0;
}
/************************************************************************/
//Purpose:  Places the accumulated character in m_Token on the list in
//			the order they were seen, and resets m_Token.
/************************************************************************/

void CCmdParse::AddNewToken()
{
	CString strNew = m_Token;
	m_pStringList->AddTail(strNew);
	memset(m_Token,'\0',sizeof(m_Token));
	m_nIndex = 0;
}

/************************************************************************/
//Purpose:  State machine and token builder.  This optionally could have
//			been broken down into separate modules for each state; however
//			due to the limited number of states and delimeters it seemed 
//			resonable to bundle it in this function.  Additional 
//			functionality would require breaking this up into constituent
//			subroutines.
//Called by:	ProcessCmdLine();
/************************************************************************/

void CCmdParse::AddChar()
{                        
   if (*m_pCurrent == '\0')
   {    //check to see if we were in the middle of building something,
   		//otherwise quit.
    	if (strlen(m_Token) > 0)
    		AddNewToken();
   		State = e_end;
   		return;
   }
   
   if (State == e_start  && (*m_pCurrent == '"'))
   {    //we are handling a quoted string
		State = e_quote;
		Previous = e_start;
		++m_pCurrent;
		return;
   }        
   
   if (State == e_start && ((*m_pCurrent != '"') && (!isspace(*m_pCurrent))) )
   {    //we are handling a non quoted string
		State = e_other;
		Previous = e_start;
		return;   	
   }
   
   if (State == e_start && (isspace(*m_pCurrent)))
   {
	   State = e_space;
       Previous = e_start;
	   return;
   }
       
   if (State == e_quote && (*m_pCurrent != '"'))   
   {    //add it to the token builder
   		m_Token[m_nIndex++] = *m_pCurrent++;
   		return;	
   }                     
   
   if (State == e_quote && (*m_pCurrent == '"'))
   {    //we found another quote.             
   		if (*(m_pCurrent -1) == '"')
   		{   //look back and see if quotes are wrong
   			//path names must be separated by quotes!!!!
   			State = e_error;
   			return;
   		}
   		if( !isspace(*(m_pCurrent +1)) && (*(m_pCurrent +1) != '\0') )
   		{
   			//look ahead and make sure they delimeted the quoted argument
   			State = e_error;
   			return;
   		}
   		Previous = State;//not used
   		//set the state so we know what we last saw and can expect
   		State = e_end_quote;
 		//add the quoted string minus the quotes
   		AddNewToken();
  		return;
   }   
   
   if (State == e_end_quote && (*m_pCurrent != '"'))
   {    //we are expecting a space or a non quote
   	   	if(isspace(*m_pCurrent))
   	   	{    
   	   		++m_pCurrent;
   	   		State = e_space;
   	   		Previous = e_end_quote;
   	   	}
   	   	else
   	   	{
   	   		State = e_other; 		
   	   		Previous = e_end_quote;
   	   	}
   	   	return; 	   		
   }
   if (State == e_end_quote && (*m_pCurrent == '"'))
   {    //suscpicious
   		++m_pCurrent; 
   		return;
   }
   
   
   if (State == e_other && ((!isspace(*m_pCurrent)) && (*m_pCurrent != '"')) )
   {    //add it to the token builder                        
    	m_Token[m_nIndex++] = *m_pCurrent++;
   		return;   		
   }
   else if(State == e_other && (isspace(*m_pCurrent)) )
   {    //add the non quoted token
   		AddNewToken();
   		Previous =  e_other;
   		if ( isspace(*m_pCurrent) )
   			State =  e_space;
   	    else
   			State = e_start;			   	    	    

		return;
    }
    else if(State == e_other && (*m_pCurrent == '"'))
    {
    	 State = e_error;
    	 return;
    }
    
    if (State == e_space && (!isspace(*m_pCurrent) ) )
    {
    	State = e_start;
    	Previous = e_space;
    	return;	
    }
    
    if (State == e_space && (isspace(*m_pCurrent)) )
    {   //eat up spaces.
    	while (isspace(*m_pCurrent))
    		++m_pCurrent;
    	return;
    }
    
     
   
   
}

/*************************************************************************/
//Purpose:	Called to start command line processing.
//			This function is used in a very minimal sense.  However, it
//			is prepared to grow such that more detailed errors and
//			potentially more states may be added to compensate for
//			additional delimiters.  Currently it simply calls AddChar()
//			and waits for an error or state end.  
//RETURNS:	True on success and False on failure.
/*************************************************************************/
BOOL CCmdParse::ProcessCmdLine()
{
	State = e_start;
	Previous = State; //may be used in the future

	m_pCurrent = &m_szParseString[0];	                          
	memset(m_Token,'\0',512);
	
	while(State != e_end)
	{
		switch(State)
		{  
			//   
			case e_error:
				return FALSE;
			  
			default:
				AddChar();
				 
		}
	}
    return TRUE;		
}
	
