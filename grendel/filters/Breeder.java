/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the GFilt filter package, as integrated into 
 * the Grendel mail/news reader.
 * 
 * The Initial Developer of the Original Code is Ian Clarke.  
 * Portions created by Ian Clarke are
 * Copyright (C) 2000 Ian Clarke.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

package grendel.filters;  // formerly GFilt

public class Breeder
{
  
  public static void main(String[] args)
  {
    /**
     * A list of strings that should "pass" the filter
     **/
    String[] pos = {"sdfviawnasdvb","asdciainasdf","erpianwdr"};

    /**
     * A list of strings that should "fail" the filter
     **/
    String[] neg = {"sdfviarnasdvb","asdciwanasdf","eriyanwdr"};

    System.out.println("Best: "+evolve(pos, neg, 500));
  }

  public static Filter evolve(String[] pos, String[] neg, int popSz)
  {
    Breeder b = new Breeder(pos,neg,popSz);
    float bestScore = 0, lastBestScore = -1;
    int BSrun=0;
    for (int x=0; (x<1000 && BSrun < 20); x++)
      {
        b.scoreAll();
        bestScore = b.getBestScore();
        if (bestScore == lastBestScore)
          {
            BSrun++; 
          }
        else
          {
            BSrun = 0;
            lastBestScore = bestScore;
          }
        System.out.println(""+x+":"+bestScore+":"+b.getBest());
        b.updatePop();
      }
    return b.getBest();
  }
  
  protected String[] positives, negatives;
  protected float[] scores;
  protected int totalScore = 0;
  protected Filter[] population;
  protected java.util.Random r = new java.util.Random();
  protected Mutator mut;
  
  public Breeder(String[] p, String[] n, int popSz)
  {
    this(p, n, null);
    population = new Filter[popSz];
    for (int x=0; x<population.length; x++)
    {
      population[x] = new Filter();
      int np = (Math.abs(r.nextInt()) % 10) + 2;
      for (int y=0; y<np; y++)
      {
        mut.addPattern(population[x]);
      }
    }
  }
  
  public Breeder(String[] p, String[] n, Filter[] f)
  {
    positives = p;
    negatives = n;
    population = f;
    StringBuffer sb = new StringBuffer();
    for (int x=0; x<p.length; x++)
    {
      sb.append(p[x]);
    }
    mut = new Mutator(sb.toString());
  }

  public void updatePop()
  {
    Filter[] nextPop = new Filter[population.length];
    nextPop[0] = getBest();
    nextPop[1] = new Filter();
    int np = (Math.abs(r.nextInt()) % 10) + 2;
    for (int y=0; y<np; y++)
      {
        mut.addPattern(nextPop[1]);
      }
    for (int x=2; x<nextPop.length; x++)
      {
        nextPop[x] = selectRandFilt().duplicate();
        for (int p=0; p<2;p++)
	  mut.mutate(nextPop[x]);
      }
    population = nextPop;
  }

  public float getBestScore()
  {
    float bestScore = scores[0];
    for (int x=1; x<population.length; x++)
      {
        if (scores[x] > bestScore)
          {
            bestScore = scores[x];
          }
      }
    return bestScore;
  }
  
  public Filter getBest()
  {
    Filter best = population[0];
    float bestScore = scores[0];
    for (int x=1; x<population.length; x++)
      {
        if (scores[x] > bestScore)
          {
            bestScore = scores[x];
            best = population[x];
          }
      }
    return best;
  }
  
  protected Filter selectRandFilt()
  {
    int p=0, x = Math.abs(r.nextInt()) % totalScore;
    while(x>=0)
      {
        x -= scores[p];
        p++;
      } 
    return population[p-1];
  }
  
  public void scoreAll()
  {
    scores = new float[population.length];
    float s;
    totalScore = 0;
    for (int x=0; x<population.length; x++)
      {
        s = score(population[x]);
        scores[x] = s;
        totalScore += s;
      }
  }
  
  protected float score(Filter f)
  {
    int score = 0;
    for (int x=0; x<positives.length; x++)
      {
        if (f.match(positives[x]))
          score++;
      }
    for (int x=0; x<negatives.length; x++)
      {
        if (!(f.match(negatives[x])))
          score++;
      }
    return score + ((float) score/((float) (f.getTLength()+30)));
  }
}





