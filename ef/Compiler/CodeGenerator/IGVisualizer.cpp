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
/*
 	The instruction graph visulizer.
	IGVisualizer.cpp

	Peter DeSantis
*/

#if defined(DEBUG) && (defined(WIN32) || defined(USE_MESA)) && defined(IGVISUALIZE)

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glut.h>

#include "CodeGenerator.h"
#include "IGVisualizer.h"
#include "Instruction.h"
#include "Primitives.h"
#include "Vector.h"

// To allow glut callbacks to access member data
IGVisualizer* IGVisualizer::self = NULL;

IGVisualizer::
IGVisualizer()


{
  font = GLUT_STROKE_ROMAN;	
  
  zoomFactor = 2;
  cumZoom = 1;
  
  
  objectCenterX = 0;
  objectCenterY = 0;
  mouseCenterX = 0;
  mouseCenterY = 0;

  nodeLocX = 0;
  nodeLocY = 0;

  xWindowDim = 800;            // Dimension of window
  yWindowDim = 600;

  xDimGL = 10000;
  yDimGL = 10000;	

  centerX = 0;
  centerY = 0;

  INSTRUCTIONWIDTH = 1400;	// The size of the instruction itself
  INSTRUCTIONHEIGHT = 500 ;
  FRAMEWIDTH = 1450;		// Frame is the area around an instruction
  FRAMEHEIGHT = 750;			
  BORDERWIDTH = 20;	        // Border around an instruction  
  SPACEBETWEENTREES = 130;

  ARGUMENTREGISTERS = 4;  // VR <= AURGUMENTMENTREGISTERS 
  
  self = this;

  roots = new Vector<VisualInstructionRoot>;
}

IGVisualizer ::
~IGVisualizer()
{
	delete roots;
}


/* 
   Add all the roots you want to be visualized before calling visualize
*/

void IGVisualizer ::
addRoots(Vector<RootPair>& inRoots)
{

  RootPair* curRoot;
  VisualInstructionRoot* vRoot = NULL;
  for(curRoot = inRoots.begin(); curRoot < inRoots.end(); curRoot++)
    {
      DataNode*	curOutput;
      Instruction* nextInsn;
      

      if(curRoot->root->getOutgoingEdgesEnd() > curRoot->root->getOutgoingEdgesBegin())
        {
          // now walk through all vr's assigned to this producer
          VirtualRegister* 	curVR;
          for (curOutput = curRoot->root->getOutgoingEdgesBegin(); 
               curOutput < curRoot->root->getOutgoingEdgesEnd();
               curOutput++)
            { 
                  switch (curOutput->kind)
                    {
                    case vkCond:
                    case vkMemory:
                      nextInsn = curOutput->getInstructionAnnotation();
                      break;
                    default:
                      curVR = curOutput->getVirtualRegisterAnnotation();
                      assert (curVR);
                      nextInsn = curVR->getDefineInstruction();
                      break;
                    } 
                  
                  assert (nextInsn);
                  
                  vRoot = new VisualInstructionRoot;
                  vRoot->root = nextInsn;
                  vRoot->exploringAsRoot = false;
                  vRoot->locAsCSE = new Vector<TreeLocation>;
                  vRoot->currentCSE = NULL;
                  vRoot->locAsRoot = NULL;
                  
                  roots->append(*vRoot);
            }
        } 
	  
		if (curRoot->root->getInstructionRoot() != NULL) {
          nextInsn = curRoot->root->getInstructionRoot();
          
          vRoot = new VisualInstructionRoot;
          vRoot->root = nextInsn;
          vRoot->exploringAsRoot = false;
          vRoot->locAsCSE = new Vector<TreeLocation>;
          vRoot->currentCSE = NULL;
          vRoot->locAsRoot = NULL;
          
          roots->append(*vRoot);
              
        }
				
    }		
}

/*
  Internal routine which visualizes the set of roots
*/
void IGVisualizer :: 
visualizeRoots()
{

  int bottom = 0;
  int left = 0;

  VisualInstructionRoot* curRoot;
  for(curRoot = self->roots->begin(); curRoot < self->roots->end(); curRoot++)
    {
      curRoot->exploringAsRoot = true;
      curRoot->locAsRoot = &DrawTree(*(curRoot->root), bottom, left);
      bottom = curRoot->locAsRoot->top + self->SPACEBETWEENTREES;
	  curRoot->exploringAsRoot = false;
    }

}

VisualInstructionRoot* IGVisualizer ::
isInRoots(Instruction* subRoot)
{
	VisualInstructionRoot* curRoot;
	for(curRoot = self->roots->begin(); curRoot < self->roots->end(); curRoot++)
		if(curRoot->root == subRoot)
			return curRoot;
	return NULL;
}




/* 
	Internal routine to draw the tree rooted a root so that root's bottom is at yBottom 
	and the tree rooted at root extends to xLeft.
*/
TreeLocation& IGVisualizer ::
DrawTree(Instruction& root, int yBottom, int xLeft)
{
	Instruction* subRoot;		
	InstructionUse* curUse;
	TreeLocation* loc;
	TreeLocation* myLocation = new TreeLocation;
	Vector<TreeLocation> subTreeLocations;
	VisualInstructionRoot* vRoot;
	int left = xLeft;
	int top =  yBottom + self->FRAMEHEIGHT; 
   
	if(root.next == NULL)
	{
	    assert(root.prev == NULL);
		assert(root.getInstructionDefineEnd() == root.getInstructionDefineBegin() +1);
		assert(root.getInstructionUseEnd() == root.getInstructionUseBegin() +1);
		
		// Instruction has been removed from the list
		curUse = root.getInstructionUseBegin();
		subRoot = CodeGenerator::instructionUseToInstruction(*curUse);
		if(subRoot != NULL)
		{
			vRoot = isInRoots(subRoot);
			if(vRoot != NULL)  // CSE
			{ 
				loc = new TreeLocation;
				loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
				loc->right =  self->FRAMEWIDTH + left;
				loc->top =  yBottom + self->FRAMEHEIGHT;
				loc->bottom = yBottom;
				DrawSubExpressionNode(subRoot, loc->rootMiddle, loc->bottom);
				vRoot->locAsCSE->append(*loc);
				loc = new TreeLocation;
				loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
				loc->right =  self->FRAMEWIDTH + left;
				loc->top =  yBottom + self->FRAMEHEIGHT;
				loc->bottom = yBottom;
				return *loc;
			} else
			{
				PrimitiveOperation op = subRoot->getPrimitive()->getOperation();
				switch (op)
				{
					case(poPhi_M):
					case(poPhi_I):	
					case(poPhi_L):
					case(poPhi_F):
					case(poPhi_D):
					case(poPhi_A):
					case(poPhi_C):
						loc = new TreeLocation;
						loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
						loc->right =  self->FRAMEWIDTH + left;
						loc->top =  yBottom + self->FRAMEHEIGHT;
						loc->bottom = yBottom;
						DrawDummyNode(loc->rootMiddle, loc->bottom);
						return *loc;
						break;
					default:
						return DrawTree(*subRoot, yBottom, left);
						break;
				}
				
			}
		} else {	// This input is not defined
			loc = new TreeLocation;
			loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
			loc->right =  self->FRAMEWIDTH + left;
			loc->top =  yBottom + self->FRAMEHEIGHT;
			loc->bottom = yBottom;
			if( (curUse->kind == udRegister) && ((*(curUse->name.vr)).index <= self->ARGUMENTREGISTERS) )
			{
				DrawArgumentNode(loc->rootMiddle, loc->bottom);
			} else
				DrawDummyNode(loc->rootMiddle, loc->bottom);
			return *loc;
		}
	  
	}
  
	assert(root.prev != NULL);

  
	
  // Draw your sub trees
  for(curUse = root.getInstructionUseBegin(); curUse < root.getInstructionUseEnd(); curUse++)
    {
	  subRoot = CodeGenerator::instructionUseToInstruction(*curUse);

	  if(subRoot != NULL)
	  {
		vRoot = isInRoots(subRoot);
		if(vRoot != NULL)  // CSE
		{ 
			loc = new TreeLocation;
			loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
			loc->right =  self->FRAMEWIDTH + left;
			loc->top =  yBottom + (2 * self->FRAMEHEIGHT);
			loc->bottom = yBottom + self->FRAMEHEIGHT;
			DrawSubExpressionNode(subRoot, loc->rootMiddle, loc->bottom);
			vRoot->locAsCSE->append(*loc);
			loc = new TreeLocation;
			loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
			loc->right =  self->FRAMEWIDTH + left;
			loc->top =  yBottom + (2 * self->FRAMEHEIGHT);
			loc->bottom = yBottom + self->FRAMEHEIGHT;
		} else	
		{
			PrimitiveOperation op = subRoot->getPrimitive()->getOperation();
			switch (op)
			{
				case(poPhi_M):
				case(poPhi_I):	
				case(poPhi_L):
				case(poPhi_F):
				case(poPhi_D):
				case(poPhi_A):
				case(poPhi_C):
					loc = new TreeLocation;
					loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
					loc->right =  self->FRAMEWIDTH + left;
					loc->top =  yBottom + (2 * self->FRAMEHEIGHT);
					loc->bottom = yBottom + self->FRAMEHEIGHT;
					DrawDummyNode(loc->rootMiddle, loc->bottom);
					break;
				default:
					loc = &DrawTree(*subRoot, yBottom + self->FRAMEHEIGHT, left);
					break;
			}
				
		}//loc = &DrawTree(*subRoot, yBottom + self->FRAMEHEIGHT, left);		    
	  } 
	  else  // This input is not defined
      {
		  loc = new TreeLocation;
          loc->rootMiddle =  (float)self->FRAMEWIDTH/2 + left;
          loc->right =  self->FRAMEWIDTH + left;
          loc->top =  yBottom + (2 * self->FRAMEHEIGHT);
          loc->bottom = yBottom + self->FRAMEHEIGHT;
          if( (curUse->kind == udRegister) && ((*(curUse->name.vr)).index <= self->ARGUMENTREGISTERS) )
			{
				DrawArgumentNode(loc->rootMiddle, loc->bottom);
			} else
				DrawDummyNode(loc->rootMiddle, loc->bottom);
      }
      
      if(loc->top > top)
		top = loc->top;

      left = loc->right;
      subTreeLocations.append(*loc);
    }

	



  if(subTreeLocations.size() == 0)
    {
      myLocation->rootMiddle = (float)self->FRAMEWIDTH/2 + xLeft;
      myLocation->right = self->FRAMEWIDTH + xLeft;
      myLocation->top = yBottom + self->FRAMEHEIGHT;
      myLocation->bottom = yBottom;
    } else {
      myLocation->rootMiddle = subTreeLocations.front().rootMiddle + (0.5f)*(subTreeLocations.back().rootMiddle - subTreeLocations.front().rootMiddle);
      myLocation->right = subTreeLocations.back().right;
      myLocation->top = top;
      myLocation->bottom = yBottom;
    }

  DrawNode(myLocation->rootMiddle, yBottom, &root, subTreeLocations);
  
  return *myLocation;

}

/*
	Dum routine reads a file to a stream, returns size
	printPretty should use strings
*/

int fileToString(char *fileName, char *string, int len)
{

	FILE* file =  fopen(fileName, "r");
	assert(file);

	char c = fgetc(file);
	int i = 0;
	while(!feof(file) && i < (len-1))
	{
		
		if(c == 9)
			c = ' ';
		string[i] = c;
		i++;
		c = fgetc(file);
	}
	
	string[i] = '\0';
	fclose(file);	
	return i;
}






/*
	Write instruction Name and label Registers
*/

void IGVisualizer ::
LabelNode(Instruction* inst)
{
  FILE* file;
  char fileName[] = "shitFile";

  file = fopen(fileName, "w" );
  char prettyString[50];


  // Label instruction *****************************************
  glColor3f(0.0, 0.0, 0.0);
  
  glPushMatrix();	// 1
  glTranslated(3*(float)self->BORDERWIDTH, (float)self->INSTRUCTIONHEIGHT/2.5, 0.0);

  int len, i;

  inst->printPretty(file); 
  fclose(file);
  len = fileToString(fileName, prettyString, 50);

  for (i = 0; i < len; i++) 
    {
      glutStrokeCharacter(self->font, prettyString[i]);
    } 
  glPopMatrix();	// 1

  

  // Label Outputs *********************************************
  glColor3f(0.5f, 0.2f, 0.4f);
  
  char registerName[50];
  float distanceBetween;
  int vrn, j;
  InstructionDefine* curDefine;
  
  int numberOfOutputs, numberOfInputs;
  
  numberOfOutputs= inst->getInstructionDefineEnd() - inst->getInstructionDefineBegin();;
  assert(numberOfOutputs <= 1);			
  
  distanceBetween = (float)self->INSTRUCTIONWIDTH/(numberOfOutputs+1); 
  
  glPushMatrix();	// 2
  glTranslatef(0.0, 3*(float)self->BORDERWIDTH, 0.0);
  
  j=1;
  for(curDefine = inst->getInstructionDefineBegin(); curDefine < inst->getInstructionDefineEnd(); curDefine++)
   {
		
		glPushMatrix();	// 3
		glTranslatef(j*distanceBetween-100, 0.0, 0.0);

		if(curDefine->kind == udRegister)
		{
			vrn = (*(curDefine->name.vr)).index;
			sprintf(registerName, "R%d", vrn); 
		} else if (curDefine->kind == udCond)
			sprintf(registerName, "Cond");
		else
			sprintf(registerName, "Store");
		len = strlen(registerName);
		for (i = 0; i < len; i++) 
        {
          glutStrokeCharacter(self->font, registerName[i]);
        } 
		glPopMatrix();	// 3
		j++;
  }
  
  
  glPopMatrix();	// 2
  
  // Label Inputs **********************************************
   glColor3f(1.0f, 0.0f, 0.2f);
  
  InstructionUse* curUse;
  numberOfInputs = inst->getInstructionUseEnd() - inst->getInstructionUseBegin();
  distanceBetween = (float)self->INSTRUCTIONWIDTH/(numberOfInputs+1); 
  
  glPushMatrix();	// 4
  glTranslatef(0.0, self->INSTRUCTIONHEIGHT - 100 - 3*(float)self->BORDERWIDTH, 0.0);
  j=1;
  for(curUse = inst->getInstructionUseBegin(); curUse < inst->getInstructionUseEnd(); curUse++)
    {
		
      glPushMatrix();	// 5
      glTranslatef(j*distanceBetween-100, 0.0, 0.0);
      
	  if(curUse->kind == udRegister)
	  {
		vrn = (*(curUse->name.vr)).index;
		sprintf(registerName, "R%d", vrn);
	  } else if (curUse->kind == udCond)
		sprintf(registerName, "Cond");
	  else
		sprintf(registerName, "Store");
		
	  len = strlen(registerName);
	  for (i = 0; i < len; i++) 
      {
		glutStrokeCharacter(self->font, registerName[i]);
      } 
      	
      glPopMatrix();	// 5
      j++;
  }
  glPopMatrix();	// 4


}


/*
	Draw a single instruction with its bottom line centered at (xMiddle, yBottom)
*/
void IGVisualizer :: 
DrawNode(double xMiddle, int yBottom, Instruction* inst, Vector<TreeLocation>& subTrees)
{
 
  glPushMatrix();	// 1

  glTranslatef((float)xMiddle - self->INSTRUCTIONWIDTH/2, (float)yBottom, 0);
  
  // Draw Instruction body *************************************
  glColor3f(0.0, 1.0, 1.0);
  glRecti(0, 0, self->INSTRUCTIONWIDTH, self->INSTRUCTIONHEIGHT);
  
  glColor3f(1.0, 1.0, 1.0);
  glRecti(self->BORDERWIDTH, self->BORDERWIDTH, self->INSTRUCTIONWIDTH-self->BORDERWIDTH, self->INSTRUCTIONHEIGHT-self->BORDERWIDTH);


  LabelNode(inst);

  // Connect Inputs ************************************************* 
  TreeLocation* loc;
  int numberOfInputs = subTrees.size();
  int distToNextLayer = self->FRAMEHEIGHT - self->INSTRUCTIONHEIGHT;
  float jag = ((float)distToNextLayer/(numberOfInputs + 1));
  float diffMid;	
  float distanceBetween = (float)self->INSTRUCTIONWIDTH/(numberOfInputs+1); 

  glPushMatrix();	// 2
  glTranslatef(0.0, (float)self->INSTRUCTIONHEIGHT, 0.0);
  
  int j=1;
  for(loc = subTrees.begin(); loc < subTrees.end(); loc++)
    {
		
      glPushMatrix();	// 3
      glTranslatef(j*distanceBetween, 0.0, 0.0);
      
	  diffMid = loc->rootMiddle - (float)xMiddle - (j*distanceBetween - ((0.5f)*self->INSTRUCTIONWIDTH));

      glBegin(GL_LINES);
         glVertex2f(0.0f, 0.0f);
         glVertex2f(0.0f, j*jag);
		 glVertex2f(0.0f, j*jag);
	     glVertex2f(diffMid, j*jag);
         glVertex2f(diffMid, j*jag);
		 glVertex2f(diffMid, (numberOfInputs + 1)*jag);
      glEnd();

      glPopMatrix();	// 3
      j++;
    }
  glPopMatrix();	// 2

	  
  glPopMatrix();	// 1

  

}

/*
	Draw a node as a subexpression 
*/
void IGVisualizer ::
DrawSubExpressionNode(Instruction* inst, double xMiddle, double yBottom)
{
  glPushMatrix();	// 1

  glTranslatef((float)xMiddle - self->INSTRUCTIONWIDTH/2, (float)yBottom, 0);
  
  // Draw Instruction body *************************************
  glColor3f(1.0, 1.0, 0.0);
  glRecti(0, 0, self->INSTRUCTIONWIDTH, self->INSTRUCTIONHEIGHT);
  
  glColor3f(1.0, 1.0, 1.0);
  glRecti(self->BORDERWIDTH, self->BORDERWIDTH, self->INSTRUCTIONWIDTH-self->BORDERWIDTH, self->INSTRUCTIONHEIGHT-self->BORDERWIDTH);



   // Draw the node
   LabelNode(inst);

  glPopMatrix();	// 1


}



/*
  Draw a dummy "unknown" instruction  with its bottom line centered at (xMiddle, yBottom)
*/
void IGVisualizer :: 
DrawDummyNode(double xMiddle, int yBottom)
{
	char unknownMess[] = "   ???   ";

  glPushMatrix();	// 1
  
  glTranslatef((float)xMiddle - self->INSTRUCTIONWIDTH/2, (float)yBottom, 0);
 
  // Draw Instruction body *************************************
  glColor3f(1.0, 0.0, 0.0);
  glRecti(0, 0, self->INSTRUCTIONWIDTH, self->INSTRUCTIONHEIGHT);
  
  glColor3f(1.0, 1.0, 1.0);
  glRecti(5 * self->BORDERWIDTH, 5 * self->BORDERWIDTH, self->INSTRUCTIONWIDTH-(5*self->BORDERWIDTH), self->INSTRUCTIONHEIGHT-(5*self->BORDERWIDTH));

  // Write Message ******************************************
   glColor3f(1.0, 0.0, 0.0);
  glPushMatrix();	// 2
  glTranslated(3*(float)self->BORDERWIDTH, (float)self->INSTRUCTIONHEIGHT/2.5, 0.0);

  int len, i;
  len = (int) strlen(unknownMess);
  for (i = 0; i < len; i++) 
    {
      glutStrokeCharacter(self->font, unknownMess[i]);
    }
  glPopMatrix();	// 2

  glPopMatrix();        //1

}




/*
  Draw an argument node
*/
void IGVisualizer :: 
DrawArgumentNode(double xMiddle, int yBottom)
{
	char unknownMess[] = " Argument ";

  glPushMatrix();	// 1
  
  glTranslatef((float)xMiddle - self->INSTRUCTIONWIDTH/2, (float)yBottom, 0);
 
  // Draw Instruction body *************************************
  glColor3f(0.0f, 1.0f, 0.3f);
  glRecti(0, 0, self->INSTRUCTIONWIDTH, self->INSTRUCTIONHEIGHT);
  
  glColor3f(1.0f, 1.0f, 1.0f);
  glRecti(5 * self->BORDERWIDTH, 5 * self->BORDERWIDTH, self->INSTRUCTIONWIDTH-(5*self->BORDERWIDTH), self->INSTRUCTIONHEIGHT-(5*self->BORDERWIDTH));

  // Write Message ******************************************
   glColor3f(0.0f, 1.0f, 0.3f);
  glPushMatrix();	// 2
  glTranslated(3*(float)self->BORDERWIDTH, (float)self->INSTRUCTIONHEIGHT/2.5, 0.0);

  int len, i;
  len = (int) strlen(unknownMess);
  for (i = 0; i < len; i++) 
    {
      glutStrokeCharacter(self->font, unknownMess[i]);
    }
  glPopMatrix();	// 2

  glPopMatrix();        //1

}





/*
	This is the window system specific stuff for setting up the window for gl.
	Visualize is called only once and it enters a non-returning display loop.
*/



enum subMenuFields
{
	findAsRoot,
	findNextRef
};

void IGVisualizer :: visualize()
{
  int zoomMenu, rootMenu;
  char* argv ="myProgram";
  int argc = 1;

  // Initialization
  glutInit(&argc, &argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(xWindowDim, yWindowDim);
  glutCreateWindow("Instruction Graph Viewer Dulux");
  
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(-xDimGL, xDimGL, -yDimGL, yDimGL);
  
  glMatrixMode(GL_MODELVIEW);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glLineWidth(1.0);
  
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glColor3f(1.0f, 1.0f, 1.0f);

  // Add callbacks 
  glutDisplayFunc(display);
  glutMouseFunc(mouse);
  glutKeyboardFunc(keyboard);
  glutReshapeFunc(reshape);
  
  // Create the menu
  zoomMenu = glutCreateMenu(selectZoom);
  glutAddMenuEntry("X 2", 2);
  glutAddMenuEntry("X 5", 5);
  rootMenu = glutCreateMenu(selectRoot);
  glutAddMenuEntry("Goto Definition", findAsRoot);
  glutAddMenuEntry("Goto Next Reference", findNextRef);
  glutCreateMenu(mainMenu);
  glutAddSubMenu("ZoomFactor", zoomMenu);
  glutAddSubMenu("Explore Subexpression", rootMenu);
  glutAddMenuEntry("Exit", 0);
  glutAttachMenu(GLUT_RIGHT_BUTTON);
  
  glutMainLoop();
}


/*	
	The callback routines
*/

void IGVisualizer ::
display()
{
  glClear(GL_COLOR_BUFFER_BIT);
  
  glPushMatrix();  // 1
  glScaled(self->cumZoom, self->cumZoom, 1);
  glTranslated(self->centerX - self->objectCenterX + self->mouseCenterX, self->centerY - self->objectCenterY + self->mouseCenterY, 0);
   
  visualizeRoots();

  glPopMatrix();   // 1
  
  glutSwapBuffers();
}

void IGVisualizer ::
selectZoom(int msg)
{
	self->zoomFactor = float(msg);
	glutPostRedisplay(); 
}


void IGVisualizer ::
selectRoot(int msg)
{
  	VisualInstructionRoot* vRoot = findRootAtLoc(self->nodeLocX, self->nodeLocY);
	if(vRoot != NULL)
	{
		//printf("FOUND ONE\n");
		switch(msg) {
		case findAsRoot:
			self->objectCenterX = vRoot->locAsRoot->rootMiddle;
			self->objectCenterY = vRoot->locAsRoot->bottom;	
			self->mouseCenterX = 0;
			self->mouseCenterY = 0;
			break;
		case findNextRef:
			if(vRoot->currentCSE == NULL)
				if(vRoot->locAsCSE->size() != 0)
					vRoot->currentCSE = vRoot->locAsCSE->begin();
				else
					return;
			else {
				vRoot->currentCSE++;
				if(vRoot->currentCSE == vRoot->locAsCSE->end())
					vRoot->currentCSE = vRoot->locAsCSE->begin();
			}
			self->objectCenterX = vRoot->currentCSE->rootMiddle;
			self->objectCenterY = vRoot->currentCSE->bottom;	
			self->mouseCenterX = 0;
			self->mouseCenterY = 0;
			break;
                }


        }
	glutPostRedisplay(); 
}


VisualInstructionRoot* IGVisualizer ::
findRootAtLoc(double x, double y)
{

	VisualInstructionRoot* vRoot;
	TreeLocation* tree;
	for(vRoot = self->roots->begin(); vRoot < self->roots->end(); vRoot++)
	{
		if(isInNode(x, y, vRoot->locAsRoot->rootMiddle, vRoot->locAsRoot->bottom))
			return vRoot;
		for(tree = vRoot->locAsCSE->begin(); tree < vRoot->locAsCSE->end(); tree++)
			if(isInNode(x, y, tree->rootMiddle, tree->bottom))
				return vRoot;
	}

	return NULL;
}

bool IGVisualizer ::
isInNode(double x, double y, double middle, int bottom)
{
	if((x < (middle + (.5*self->INSTRUCTIONWIDTH))) && (x > (middle - (.5*self->INSTRUCTIONWIDTH))))
		if((y > bottom) && (y < (bottom + self->INSTRUCTIONHEIGHT)))
			return true;
	return false;
}




void IGVisualizer :: 
mainMenu(int msg)
{
	switch(msg) {
	case 0:
		exit(0);
		break;
	}

	glutPostRedisplay();
}

void IGVisualizer ::
mouse(int button, int state, int x, int y)
{
	
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		double transX = -(x - (.5 * (self->xWindowDim)))*((2*self->xDimGL/self->xWindowDim)/self->cumZoom);
		double transY = (y - (.5 * (self->yWindowDim)))*((2*self->yDimGL/self->yWindowDim)/self->cumZoom);
		self->mouseCenterX = self->mouseCenterX + transX;
		self->mouseCenterY = self->mouseCenterY + transY; 
	}

	glutPostRedisplay();
}

void IGVisualizer ::
keyboard(unsigned char key, int x, int y)
{
	switch(key){
	case '+':
		self->cumZoom = self->cumZoom * self->zoomFactor;
		break;
	case '-':
		self->cumZoom = self->cumZoom / self->zoomFactor;
		break;
	case 's':
		double transX = (x - (.5 * (self->xWindowDim)))*((2*self->xDimGL/self->xWindowDim)/self->cumZoom);
		double transY = -(y - (.5 * (self->yWindowDim)))*((2*self->yDimGL/self->yWindowDim)/self->cumZoom);
		
		//printf("mouse center**%f, %f**\n", self->mouseCenterX, self->mouseCenterY);
		//printf("strans      **%f, %f**\n", transX, transY);
		//printf("obj center  **%f, %f**\n", self->objectCenterX, self->objectCenterY);
		
		self->nodeLocX = transX - self->mouseCenterX + self->objectCenterX;
		self->nodeLocY = transY - self->mouseCenterY + self->objectCenterY;
		
		//printf("node center **%f, %f**\n", self->nodeLocX, self->nodeLocY);
		break;	
	}
	glutPostRedisplay();
}

void IGVisualizer :: 
reshape(int width, int height)
{
	self->xWindowDim = width;
	self->yWindowDim = height;

	glViewport(0, 0, width, height);
	glutPostRedisplay();
}

#endif
